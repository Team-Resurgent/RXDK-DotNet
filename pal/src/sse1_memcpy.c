/*
 * Copyright (c) 2026 wutno (aaron@installgentoo.net)
 * Copyright (c) 2026 Claude (Anthropic)
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * sse1_memcpy.c -- hand-rolled memcpy tuned for the Pentium III (P6 core:
 *                  32B lines, 16K L1D, no hardware prefetcher, 4 line-fill
 *                  buffers, and -- measured on the Xbox Coppermine -- only
 *                  ONE line combining in the WC buffers at a time).
 *                  SSE1 instructions only: no SSE2, no MMX register use.
 *
 * Entry points:
 *   sse1_memcpy(dst, src, n)     -- cached stores below NT_THRESHOLD (rep
 *                                   movsd fast strings from REP_MIN up, an
 *                                   SSE loop below that), block prefetch +
 *                                   movntps at/above it
 *   sse1_memcpy_nt(dst, src, n)  -- block-prefetch + movntps for every n > 64:
 *                                   for destinations the CPU will not read
 *                                   back soon (vertex/texture uploads etc.);
 *                                   0.79 cyc/B from an L2-resident source vs
 *                                   1.40 for cached stores, sfence included
 *
 * Build:  clang -m32 -march=pentium3 -O2 -c sse1_memcpy.c
 *         (any target with -msse works; -mmmx is not required)
 *
 * Why SSE1 __m128 intrinsics and not MMX intrinsics:
 *   Since Clang 19 every __m64/MMX intrinsic (mmintrin.h, plus the __m64
 *   parts of xmmintrin.h such as _mm_stream_pi/_m_movntq) is lowered to
 *   SSE2 instructions and refuses to compile with -march=pentium3 /
 *   -mno-sse2.  Real MMX is only reachable through inline asm on Clang 19+.
 *   The __m128 intrinsics below still lower to plain SSE1:
 *     movaps movlps movhps movntps sfence
 *   No x87 interaction, so no emms is needed anywhere.  The only non-
 *   intrinsic code is four 1-2 instruction movlps/movhps asm helpers
 *   (ld8/st8/ld16u/st16u); -DSSE1_MEMCPY_NO_ASM swaps them for the
 *   intrinsic forms (clang then adds an xorps before every movlps load).
 *
 * Size classes (n = bytes):
 *   <= 16          : loop-free overlapping scalar / movlps moves
 *   <= 64          : 2 or 4 overlapping unaligned 16B moves
 *   >  64          : 32B unaligned head, dst aligned to a 32B line, 64B
 *                    (two-line) blocks, aligned tail + one overlapping
 *                    unaligned 16B move.  Block loop by size:
 *     <  REP_MIN   : movaps stores, no prefetch (dst stays cache-resident)
 *     <  NT_THR    : rep movsd -- P6 fast strings; dst is 32B-aligned and
 *                    the count is a whole number of lines, which is what
 *                    the fast-string mode wants
 *     >= NT_THR    : block prefetch -- touch a BP_BLOCK chunk of src into
 *                    L1 with one byte load per line (up to 4 misses in
 *                    flight, never dropped on a DTLB miss), then stream it
 *                    out with movntps one full line at a time; sfence at
 *                    the end so DMA/GPU-visible memory is coherent on return
 *
 * Measured on the Xbox (733 MHz Coppermine, 128K L2, 133 MHz FSB; see
 * bench_p3_claims.c), and what each number decided:
 *   - Cold 1 MB copy: block prefetch 1.65 cyc/B (8K block 1.60), any in-loop
 *     variant 3.1-3.4, rep movsd 2.9.  From an L2-resident source: 0.79 vs
 *     1.40 for cached stores.  => one NT path, block prefetch, 8K blocks.
 *   - Software prefetch (prefetchnta, any distance 64..1024) gains nothing
 *     and costs up to 8%: the 4 fill buffers are already saturated by the
 *     demand loads, a prefetch that finds them busy stalls issue for ~38
 *     cycles, and one that misses the DTLB is dropped.  => no prefetch.
 *   - Only one WC line combines at a time: writing a second line before
 *     the first is complete evicts it as two partial-line bus writes.
 *     => two consecutive movntps per 32B line, lines never interleaved,
 *     block loop starts on a 32B boundary.
 *   - Misaligned movups store: 10.0 clk (9 uops); movlps+movhps pair: 2.4
 *     (4 uops).  movups load: 2.0 clk like movaps but 4 uops vs 2.  Pair
 *     load: 2.0 clk, 2 uops, and an 8B access only pays the ~7 clk split
 *     when it actually crosses a line.  => pairs for every unaligned move.
 *   - xmm halves are renamed independently (movlps to the same xmm back to
 *     back: 1.0 clk), and the xorps clang adds in front of _mm_loadl_pi
 *     costs a full clock per pair.  => the asm pair helpers, no zeroing.
 *   - Cached copies: rep movsd beats the movaps loop everywhere measured --
 *     0.57 vs 1.38 cyc/B from an L2-resident source, 1.85 vs 2.06 cold --
 *     and leaves dst cached like any store.  The movaps loop commits one
 *     RFO at a time out of the 12-entry store buffer; fast strings do not.
 *     => rep movsd for the cached path from REP_MIN up.
 *   - Cached vs block prefetch, cold source: BP copies faster from 16K up
 *     (1.59 vs 1.85) but leaves dst uncached (+0.9 cyc/B to read it back);
 *     with the read-back included the crossover is 128K.  => NT_THRESHOLD
 *     128K; use sse1_memcpy_nt when the destination is not read back.
 *   - Against the NXDK libc memcpy (a byte loop, 3 cyc/B hot, 7 cold): 4.5x
 *     on cold copies of any size, 5x from L2, 3-13x on hot 8..4096 B.  The
 *     only place it won was 1..3 B (21 vs 30 cycles per call): the whole
 *     body in one function cost four callee-saved pushes on every call, so
 *     the n <= 64 paths now live in the exported functions and the rest is
 *     out of line behind a tail jump (a plain call cost +15 cycles).
 *   - rep movsd with a source that is not dword-aligned ran at 0.82 cyc/B
 *     hot against 0.6 for the pair loop.  => rep only when (src & 3) == 0
 *     after the head alignment; otherwise the movaps/pair loop.
 *   - Stride-64 stores and no byte loops keep LLVM's loop-idiom pass from
 *     turning any of this back into a memcpy() call.
 *
 * Caveat: block prefetch assumes a cacheable (WB) source.  For a WC/UC
 * source (e.g. reading back GPU memory) the touch loop would double the bus
 * traffic: build with SSE1_MEMCPY_NT_THRESHOLD above any size you use, or
 * copy such sources with the cached path only.
 *
 * Semantics: memcpy, not memmove.  dst/src must not overlap (except the
 * trivial dst == src case, which is harmless).  Returns dst.
 */

#include <stddef.h>
#include <stdint.h>
#include <xmmintrin.h>

/* Bytes at/above which sse1_memcpy switches from cached stores to block
 * prefetch + movntps.  Measured against rep movsd with the destination read
 * back afterwards: tie at 128K, BP ahead from 192K; without read-back BP is
 * ahead from 16K (use sse1_memcpy_nt for that case). */
#ifndef SSE1_MEMCPY_NT_THRESHOLD
#define SSE1_MEMCPY_NT_THRESHOLD (128u * 1024u)
#endif

/* Cached path: rep movsd (P6 fast strings) from REP_MIN bytes up, the movaps
 * loop below.  Fast strings need dst 8B-aligned (we give 32) and DF=0 (ABI).
 * REP_MIN covers the fast-string start-up; measured only from 16K so far,
 * bench T12 sizes it.  SSE1_MEMCPY_CACHED_REP=0 restores the loop. */
#ifndef SSE1_MEMCPY_CACHED_REP
#define SSE1_MEMCPY_CACHED_REP 1
#endif
#ifndef SSE1_MEMCPY_REP_MIN
#define SSE1_MEMCPY_REP_MIN 512u
#endif

/* Block-prefetch chunk in bytes, multiple of 64.  Must sit in the 16K L1D:
 * measured 1K 1.82, 2K 1.70, 4K 1.65, 8K 1.60 cyc/B on a cold 1 MB copy. */
#ifndef SSE1_MEMCPY_BP_BLOCK
#define SSE1_MEMCPY_BP_BLOCK 8192u
#endif

#define SSE1_AI static inline __attribute__((always_inline))

/* Unaligned, alias-safe 8-byte lvalue for the asm memory operands. */
typedef uint64_t __attribute__((may_alias, aligned(1))) u64_ua;

/* ---- 8B / 16B move primitives ------------------------------------------ */

#ifndef SSE1_MEMCPY_NO_ASM
/* asm rather than _mm_loadl_pi(_mm_undefined_ps(), ..): clang re-materialises
 * an xorps zeroing before every such load inside a loop, and on P3 the two
 * xmm halves are separate physical registers so the zeroing is pure waste.
 * {att|intel} dialect alternatives keep the asm valid under -masm=intel. */
SSE1_AI __m128 ld8(const unsigned char *s)                      /* movlps        */
{
    __m128 v;
    __asm__("movlps {%1, %0|%0, %1}" : "=x"(v) : "m"(*(const u64_ua *)s));
    return v;
}
SSE1_AI void st8(unsigned char *d, __m128 v)                    /* movlps        */
{
    __asm__("movlps {%1, %0|%0, %1}" : "=m"(*(u64_ua *)d) : "x"(v));
}
SSE1_AI __m128 ld16u(const unsigned char *s)                    /* movlps+movhps */
{
    __m128 v;
    __asm__("movlps {%1, %0|%0, %1}\n\tmovhps {%2, %0|%0, %2}"
            : "=x"(v)
            : "m"(*(const u64_ua *)s), "m"(*(const u64_ua *)(s + 8)));
    return v;
}
SSE1_AI void st16u(unsigned char *d, __m128 v)                  /* movlps+movhps */
{
    __asm__("movlps {%2, %0|%0, %2}\n\tmovhps {%2, %1|%1, %2}"
            : "=m"(*(u64_ua *)d), "=m"(*(u64_ua *)(d + 8))
            : "x"(v));
}
#else
SSE1_AI __m128 ld8(const unsigned char *s)
{
    return _mm_loadl_pi(_mm_undefined_ps(), (const __m64 *)s);
}
SSE1_AI void st8(unsigned char *d, __m128 v)
{
    _mm_storel_pi((__m64 *)d, v);
}
SSE1_AI __m128 ld16u(const unsigned char *s)
{
    return _mm_loadh_pi(_mm_loadl_pi(_mm_undefined_ps(), (const __m64 *)s),
                        (const __m64 *)(s + 8));
}
SSE1_AI void st16u(unsigned char *d, __m128 v)
{
    _mm_storel_pi((__m64 *)d,       v);
    _mm_storeh_pi((__m64 *)(d + 8), v);
}
#endif

SSE1_AI __m128 ld16a(const unsigned char *s)                    /* movaps        */
{
    return _mm_load_ps((const float *)s);
}
SSE1_AI void st16a(unsigned char *d, __m128 v)                  /* movaps        */
{
    _mm_store_ps((float *)d, v);
}
SSE1_AI void st16nt(unsigned char *d, __m128 v)                 /* movntps       */
{
    _mm_stream_ps((float *)d, v);
}

/* ---- 64B block loops ---------------------------------------------------- */

enum { SRC_A16 = 0, SRC_PAIR = 1 };     /* src 16-aligned: movaps, else pair */
enum { ST_CACHED = 0, ST_NT = 1 };      /* movaps vs movntps stores          */

/* One two-line block: 4 loads, then 4 stores.  src/st are compile-time
 * constants at every call site, so each loop below is fully specialised. */
SSE1_AI void blk64(unsigned char *d, const unsigned char *s, int src, int st)
{
    __m128 v0 = (src == SRC_A16) ? ld16a(s)      : ld16u(s);
    __m128 v1 = (src == SRC_A16) ? ld16a(s + 16) : ld16u(s + 16);
    __m128 v2 = (src == SRC_A16) ? ld16a(s + 32) : ld16u(s + 32);
    __m128 v3 = (src == SRC_A16) ? ld16a(s + 48) : ld16u(s + 48);
    if (st == ST_NT) {
        st16nt(d, v0); st16nt(d + 16, v1); st16nt(d + 32, v2); st16nt(d + 48, v3);
    } else {
        st16a(d, v0);  st16a(d + 16, v1);  st16a(d + 32, v2);  st16a(d + 48, v3);
    }
}

/* Cached loop: movaps stores, no prefetch (measured: prefetchnta at any
 * distance is neutral to -8% here; the demand loads already keep the 4 fill
 * buffers busy). */
SSE1_AI void loop_cached(unsigned char *d, const unsigned char *s, size_t nblk,
                         int src)
{
    do {
        blk64(d, s, src, ST_CACHED);
        d += 64;
        s += 64;
    } while (--nblk);
}

/* Pull [s, s+bytes) into L1: one byte load per 32B line.  A byte load never
 * reads past the chunk, volatile keeps the loads, and a real load (unlike
 * prefetchnta) walks the page tables on a DTLB miss instead of being dropped.
 * Any cacheable load fills the whole line. */
SSE1_AI void touch(const unsigned char *s, size_t bytes)
{
    uintptr_t p = (uintptr_t)s, e = p + bytes;
    (void)*(const volatile unsigned char *)p;                   /* line of s  */
    for (p = (p | 31) + 1; p < e; p += 32)                       /* next lines */
        (void)*(const volatile unsigned char *)p;
}

/* Block prefetch: read a BP_BLOCK chunk into L1 (4 misses in flight), then
 * stream it out with movntps from L1, one complete 32B line at a time. */
SSE1_AI void loop_bp(unsigned char *d, const unsigned char *s, size_t nblk,
                     int src)
{
    do {
        size_t cnt = nblk < (SSE1_MEMCPY_BP_BLOCK / 64) ? nblk
                                                         : (SSE1_MEMCPY_BP_BLOCK / 64);
        size_t i = cnt;
        touch(s, cnt << 6);
        do {
            blk64(d, s, src, ST_NT);
            d += 64;
            s += 64;
        } while (--i);
        nblk -= cnt;
    } while (nblk);
}

/* Cached path via P6 fast strings: whole 64B blocks, dst 32B-aligned. */
SSE1_AI void loop_rep(unsigned char *d, const unsigned char *s, size_t nblk)
{
    size_t cnt = nblk << 4;                                     /* dwords */
    __asm__ volatile("rep {movsl|movsd}"
                     : "+D"(d), "+S"(s), "+c"(cnt) : : "memory");
}

/* d is 32-aligned, nblk >= 1.  nt: block prefetch + movntps; rep: fast
 * strings; else the movaps loop. */
SSE1_AI void copy_body(unsigned char *d, const unsigned char *s, size_t nblk,
                       int nt, int rep)
{
    int a16 = ((uintptr_t)s & 15) == 0;
    if (nt) {
        if (a16) loop_bp(d, s, nblk, SRC_A16);
        else     loop_bp(d, s, nblk, SRC_PAIR);
        _mm_sfence();
    } else if (rep) {
        loop_rep(d, s, nblk);
    } else {
        if (a16) loop_cached(d, s, nblk, SRC_A16);
        else     loop_cached(d, s, nblk, SRC_PAIR);
    }
}

/* force_nt is a compile-time constant at both call sites below. */
/* ---- n <= 64: lives in the exported functions, so their prologue stays a
 * few instructions (no callee-saved pushes: measured 30 cycles per 1-byte
 * call with the whole body in one function, the libc byte loop 21).  Only
 * eax/ecx/edx and xmm are needed here, which is why the 4..7 case goes
 * through movss instead of two GPR temporaries. ---------------------- */
SSE1_AI int copy_small(unsigned char *d, const unsigned char *s, size_t n)
{
    if (n <= 16) {
        if (n >= 8) {                                           /* 8..16  */
            __m128 a = ld8(s), b = ld8(s + n - 8);
            st8(d,         a);
            st8(d + n - 8, b);
        } else if (n >= 4) {                                    /* 4..7   */
            __m128 a = _mm_load_ss((const float *)s);           /* movss  */
            __m128 b = _mm_load_ss((const float *)(s + n - 4));
            _mm_store_ss((float *)d,           a);
            _mm_store_ss((float *)(d + n - 4), b);
        } else if (n) {                                         /* 1..3   */
            d[0]      = s[0];
            d[n >> 1] = s[n >> 1];
            d[n - 1]  = s[n - 1];
        }
        return 1;
    }
    if (n <= 32) {                                              /* 17..32 */
        __m128 a = ld16u(s), b = ld16u(s + n - 16);
        st16u(d,          a);
        st16u(d + n - 16, b);
        return 1;
    }
    if (n <= 64) {                                              /* 33..64 */
        __m128 a = ld16u(s),          b = ld16u(s + 16);
        __m128 c = ld16u(s + n - 32), e = ld16u(s + n - 16);
        st16u(d,          a);
        st16u(d + 16,     b);
        st16u(d + n - 32, c);
        st16u(d + n - 16, e);
        return 1;
    }
    return 0;
}

/* ---- n > 64: 32B head, align dst to a 32B line, blocks, tail ------------ */
SSE1_AI void *copy_big(void *restrict dst, const void *restrict src, size_t n,
                       int force_nt)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    int nt = force_nt || n >= SSE1_MEMCPY_NT_THRESHOLD;         /* on the caller's n */

    {
        __m128 a = ld16u(s), b = ld16u(s + 16);
        size_t adj = 32 - ((uintptr_t)d & 31);                  /* 1..32  */
        st16u(d,      a);
        st16u(d + 16, b);
        d += adj;
        s += adj;
        n -= adj;                                               /* >= 33  */
    }

    if (n >> 6) {
        size_t nblk = n >> 6;
        /* rep movsd only with a dword-aligned source: misaligned, the fast-
         * string microcode runs at 0.82 cyc/B hot vs 0.6 for the pair loop */
        int rep = SSE1_MEMCPY_CACHED_REP && n >= SSE1_MEMCPY_REP_MIN &&
                  ((uintptr_t)s & 3) == 0;
        copy_body(d, s, nblk, nt, rep);
        d += nblk << 6;
        s += nblk << 6;
        n &= 63;
    }

    /* Tail: 0..63 bytes.  Whole 16B chunks land on the aligned dst; the
     * final <16 bytes are one unaligned move overlapping what we just wrote
     * (identical bytes, so the overlap is benign even on top of movntps). */
    if (n & 32) {
        __m128 a = ld16u(s), b = ld16u(s + 16);
        st16a(d,      a);
        st16a(d + 16, b);
        d += 32;
        s += 32;
    }
    if (n & 16) {
        st16a(d, ld16u(s));
        d += 16;
        s += 16;
    }
    if (n & 15) {
        size_t r = n & 15;
        st16u(d + r - 16, ld16u(s + r - 16));
    }
    return dst;
}

/* Out of line so the exported functions carry no prologue for the small
 * paths.  Reached by a forced tail jump (musttail): a plain call cost the
 * n > 64 paths a second frame, measured +15 cycles per call. */
static __attribute__((noinline))
void *copy_big_auto(void *restrict dst, const void *restrict src, size_t n)
{
    return copy_big(dst, src, n, 0);
}
static __attribute__((noinline))
void *copy_big_nt(void *restrict dst, const void *restrict src, size_t n)
{
    return copy_big(dst, src, n, 1);
}

void *sse1_memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    if (copy_small((unsigned char *)dst, (const unsigned char *)src, n))
        return dst;
    __attribute__((musttail)) return copy_big_auto(dst, src, n);
}

/* Same copy, but block prefetch + movntps for every n > 64: for destinations
 * the CPU will not read back soon.  Ends with sfence. */
void *sse1_memcpy_nt(void *restrict dst, const void *restrict src, size_t n)
{
    if (copy_small((unsigned char *)dst, (const unsigned char *)src, n))
        return dst;
    __attribute__((musttail)) return copy_big_nt(dst, src, n);
}
