# RXDK-DotNet — porting a managed .NET runtime to the original Xbox

**Status: planning / scoping.** Nothing built yet. This document is the plan; the runtime
choice (Mono vs NativeAOT) is deliberately **left open** and decided at a gate in Phase 0
after a targeted spike (see [§4](#4-the-pivotal-constraint-sse2) and [§7](#7-decision-gate)).

Goal: get **managed .NET code executing on retail original Xbox hardware**, as a lightweight
runtime layered on the **RXDK-Libs kernel-imports foundation** (`libkernel` + `libc` +
`libcpp`), with a Platform Abstraction Layer (PAL) bound **directly to `xboxkrnl` exports** and
no dependency on xAPI / D3D8 / the rest of the subsystem stack.

---

## 1. What we inherit from RXDK-Libs (this is why the port is feasible)

A normal "bring a managed runtime to bare metal" effort spends most of its time building the
C runtime, the linker/toolchain story, and the low-level OS glue. **RXDK-Libs already did all
of that and HW-validated it.** We build on it rather than reinventing it.

| Runtime need | Provided by RXDK-Libs | Notes |
|---|---|---|
| Cross toolchain | clang/lld, `i686-pc-windows-gnu`, `-march=pentium3` | Managed install via `rxdk install-llvm`; PIII-pinned |
| C runtime | `libc.lib` (picolibc + Xbox HAL) | freestanding, C23 |
| C++ runtime | `libcpp.lib` (LLVM libc++ + libc++abi) | freestanding profile |
| Kernel imports | `libkernel` from `xboxkrnl.def` | direct `Nt*/Ke*/Ps*/Mm*/Ex*/Rtl*` — **our PAL target** |
| Heap | `RtlAllocateHeap` on `XapiProcessHeap`, 16-byte aligned | `malloc`/`operator new` already routed here |
| Threads | C11 `threads.h` on `PsCreateSystemThreadEx` | `libc/xbox/threads.c` — recursive `mtx_t`, `cnd_t` on `KEVENT` |
| Exceptions | DWARF/Itanium unwinding (libunwind + libc++abi) | `.eh_frame`; `main` runs on a `PsCreateSystemThreadEx` thread (init stack too small for the unwinder) |
| compiler-rt | `libclang_rt.builtins-i386.a` | `__divdi3`, `__alloca`, `__chkstk`, int↔fp helpers |
| ISA safety gate | `tools/isa-scan.py` (Capstone) | fails the build on any post-SSE1 opcode — **reuse verbatim** |
| Packaging | RXDK-Tools `imagebld` → XBE (subsystem 14) | coerces subsystem, zero-fills `.bss`, resolves TLS dir |

**Bottom line:** the "boot an ELF/PE freestanding, print, allocate, thread, unwind" layer is
solved. Our new work is almost entirely **above** that line: a .NET PAL, the GC's OS bindings,
the AOT/JIT image pipeline, and a trimmed corlib.

---

## 2. Target hardware constraints

| Constraint | Value | Implication for a managed runtime |
|---|---|---|
| CPU | Intel Pentium III "Coppermine", 733 MHz, 1 core | **MMX + SSE1 only, NO SSE2.** Single hardware thread — real preemptive threads exist, but no true parallelism |
| **SSE2** | **absent — faults `STATUS_ILLEGAL_INSTRUCTION` on real HW** | The dominant blocker. .NET x86 codegen assumes SSE2. See §4. xemu **masks** this — real-HW testing is mandatory |
| RAM (retail) | 64 MB unified (CPU+GPU) | Tight. corlib metadata + GC heap + runtime image must all fit. This bounds how much BCL we can carry |
| RAM (devkit) | 128 MB | Dev headroom, but ship target is 64 MB |
| NX bit | **none** (PIII) | Code executes from any writable page → **a JIT is architecturally permitted** (write code, jump to it). No W^X dance needed |
| FPU | x87 (80-bit) + SSE1 (scalar single only, no double) | x87 gives IEEE-ish `double`; **`double` in SSE requires SSE2** — so scalar `double` math must be x87 on this CPU |
| Kernel | custom `xboxkrnl.exe` (NT-subset) | No `kernel32`/`ntdll` — PAL binds to raw `Nt*/Ke*/Mm*/Ps*` exports (we have them all) |

---

## 3. The kernel-imports surface a PAL can bind to

Confirmed available in `shared/include/xboxkrnl/api/*` and `xboxkrnl.def`. This is the entire
lower edge of the PAL:

| PAL concern | xboxkrnl exports available |
|---|---|
| **Virtual memory (GC)** | `NtAllocateVirtualMemory` (MEM_RESERVE/COMMIT), `NtFreeVirtualMemory` (DECOMMIT/RELEASE), `NtProtectVirtualMemory`, `NtQueryVirtualMemory` — **full reserve→commit + protection**, i.e. a `VirtualAlloc` equivalent |
| System / contiguous memory | `MmAllocateSystemMemory` (+Protect), `MmSetAddressProtect`, `MmGetPhysicalAddress`, `MmQueryAllocationSize`, `MmAllocateContiguousMemory` |
| **Threads** | `PsCreateSystemThreadEx` (stack/TLS sizing, suspended-start, system-routine), `PsCreateSystemThread`, `PsSetCreateThreadNotifyRoutine`, `PsThreadObjectType` |
| Thread control | `KeSuspendThread`/`KeResumeThread`, `KeSetPriorityThread`, `KeGetCurrentThread`, `KeAlertThread` |
| **Synchronization** | `KeInitializeEvent`/`KeSetEvent`/`KeResetEvent`/`KePulseEvent`, `KeInitializeSemaphore`/`KeReleaseSemaphore`, `KeInitializeMutant`/`KeReleaseMutant`, `KeWaitForSingleObject`, `KeWaitForMultipleObjects`; NT-object equivalents `NtCreateEvent/Semaphore/Mutant`, `NtWaitForSingleObject(Ex)`, `NtDuplicateObject`, `NtClose`; RTL critical sections (already wrapped as C11 `mtx_t`) |
| Blocking / sleep | `KeDelayExecutionThread`, `KeStallExecutionProcessor` (spin) |
| **Timing** | `KeQueryPerformanceCounter`/`Frequency`, `KeQuerySystemTime`, `KeTickCount`, `KeInterruptTime` |
| **FP state (GC suspend / context switch)** | `KeSaveFloatingPointState` / `KeRestoreFloatingPointState` (x87/SSE) |
| File / stream I/O | `NtCreateFile`, `NtReadFile(Scatter)`, `NtWriteFile(Gather)`, `NtClose` |
| Debug output | `DbgPrint` (import ordinal 8) — the "Console.Out" of bring-up |
| APC / DPC (GC coop-suspend, callbacks) | `KeInitializeApc`/`KeInsertQueueApc`, `KeInitializeDpc`/`KeInsertQueueDpc` |

**Gaps to close ourselves (flagged early):**

- **Thread-local storage — works, but wrong shape for a runtime hot path.** TLS is *not*
  unfinished (the stale header comment in `threads.c` lines 17–18 contradicts the code below
  it): `tss_*` is fully implemented and `emutls.c` provides a complete emulated-TLS runtime, so
  clang `thread_local` / libc++ thread-locals work. **But** the implementation is a flat global
  table keyed by `KeGetCurrentThread()` — `tss_get`/`tss_set` do an **O(n) linear scan of 256
  slots under one global critical section** (`threads.c:559–599`), with fixed capacity (32 keys /
  256 system-wide slots, `EMUTLS_MAX = 256` per thread). RXDK creates threads with
  `PsCreateSystemThreadEx(..., TlsDataSize = 0, ...)` and stubs the native model
  (`tls_stub.c`: `__x86_tls_tcb_offset = 0`, `__set_tcb` no-op), so there is no register-relative
  TLS. A managed runtime reads the current-thread/GC-state pointer on every managed↔native
  transition, GC safepoint, and EH walk — a global-lock linear scan there is a real bottleneck.
  **We must provide true per-thread storage**: TEB-backed (Xbox has `fs:`→TEB like NT) or via the
  `TlsDataSize` block of `PsCreateSystemThreadEx`, for O(1) register-relative access. On the
  critical path.
- **GC thread suspension.** Cooperative-mode GC (both runtimes' preference on constrained
  platforms) needs safepoint polling; preemptive suspension would need `KeSuspendThread` +
  reading a saved thread context, which the Xbox kernel does not cleanly expose for arbitrary
  register capture. **Plan for cooperative-mode GC.**

---

## 4. The pivotal constraint: SSE2

This decides the architecture, so it gets spiked *first*.

- **.NET's modern x86 codegen (RyuJIT, and therefore NativeAOT's ILC backend) has a hard SSE2
  floor.** Scalar `float`/`double` lower to `movss`/`movsd`/`addsd` on `xmm` registers; there
  is no `-march=pentium3` knob and no x87 fallback path. On a real Xbox those instructions
  raise `STATUS_ILLEGAL_INSTRUCTION`. **This is the single biggest risk in the whole project.**
- **Mono's classic x86 backend (`mini-x86.c`) can generate x87 FPU code** (`MONO_ARCH_USE_FPSTACK`
  lineage) — it ran on Pentium-class hardware historically (and shipped on PS3/Wii/iOS via AOT).
  This makes Mono the only candidate with a *plausible* path to running on a PIII **without**
  compiler surgery. ⚠️ **Must verify:** confirm the specific Mono version/config we pick still
  emits x87 (or SSE1-scalar) for x86 and can be pinned there — newer Mono may default to SSE2.

**Three mitigation paths for the NativeAOT branch** (all researchy):
1. **Patch RyuJIT's ISA floor** to emit x87 for scalar FP on a "pentium3" target. Large,
   invasive, upstream-divergent; FP is pervasive in codegen.
2. **SSE2 trap-emulator**: a `STATUS_ILLEGAL_INSTRUCTION` handler that decodes and emulates the
   offending SSE2 ops. Correct but *slow*; viable as a bring-up crutch, not a ship strategy.
3. **Accept it and don't ship NativeAOT** — treat NativeAOT as the "if we ever fix codegen"
   track and ship Mono.

The isa-scan gate (`tools/isa-scan.py`) is how we *measure* this: run it over the AOT output
object files. Any hit = won't boot on HW. This is a hard CI gate from day one.

---

## 5. Runtime candidates compared

| Dimension | **Mono (full-AOT or mini-JIT)** | **NativeAOT / CoreRT** | CoreCLR + RyuJIT (reference only) |
|---|---|---|---|
| x86 FP / ISA | **x87 possible** → PIII-clean (verify) | SSE2-hard → **won't boot** without §4 work | SSE2-hard → won't boot |
| Model | AOT images, or JIT (Xbox allows it — no NX) | single native binary via ILC (no JIT) | JIT everything at runtime |
| "Runs on real HW without codegen surgery" | **Yes (expected)** | No | No |
| Memory footprint | Small–medium; tunable, interp fallback | Small binary, but ILC-linked corlib still sizable | Largest (JIT + full corlib) |
| PAL size | Small, well-documented `mono/utils` + `sgen` OS hooks | Medium (Redhawk PAL) | Large (full PAL) |
| Metadata / reflection | Full (Mono metadata) even under AOT | Limited under AOT (trimming, no dynamic codegen) | Full |
| Licensing | MIT | MIT | MIT |
| Build host | mono cross-AOT compiler → i686 | ILC cross-compile → i686 obj | n/a |
| Philosophical fit ("`.NET Native`") | "a .NET runtime on Xbox" | **literal "Microsoft .NET Native"** | — |

**Working recommendation (to be confirmed at the gate):** **Mono** is the realistic first
bootable target because of x87. Because the Xbox has **no NX bit, Mono's mini-JIT is also on
the table** (write code to a normal page and jump) — which sidesteps full-AOT's
generic-instantiation coverage gaps. NativeAOT stays a parallel research track gated on §4;
if the SSE2 problem is ever solved, it is the more elegant, "native", low-footprint answer.

All three are MIT-licensed — combining with the GPLv3 RXDK-Libs base is one-directional and
fine (the combined work is GPLv3).

---

## 6. Architecture

```
        managed title (IL, AOT-compiled and/or JIT'd on device)
                              │
                    trimmed corlib / BCL subset
                              │
        ┌─────────────────────────────────────────────┐
        │  managed runtime (Mono mini + sgen GC,  OR   │
        │  NativeAOT Redhawk runtime + GC)             │
        └─────────────────────────────────────────────┘
                              │
        ┌─────────────────────────────────────────────┐
        │  RXDK-DotNet PAL  (new — this project)       │
        │  memory · threads · sync · TLS · time · I/O  │
        └─────────────────────────────────────────────┘
                              │
   libkernel (xboxkrnl imports)   +   libc / libcpp (RXDK-Libs)
                              │
                     xboxkrnl.exe  (real Xbox kernel)
                              │
                        imagebld → .xbe
```

The PAL is the heart of the new work. Everything under it exists; everything above it is
upstream runtime source we port/trim. The PAL maps 1:1 onto the tables in §3.

---

## 7. Decision gate

### Direction chosen (2026-09-27): **Mono, phased** — classic now, net8+ via the interpreter later

The runtime question is effectively resolved to **Mono** (NativeAOT is shelved — its RyuJIT
backend can't target a Pentium III; see §4). Two phases:

- **Phase A — classic Mono** (`mono/mono`, forked to **`EqUiNoX-Labs/mono`**, branch **`xbox`**
  @ `0f53e9e`, wired in at `vendor/mono`). x87-native, self-contained BCL (Framework 4.x /
  netstandard2.0). This is the bring-up vehicle: prove the PAL, x87 codegen, the
  `IL → .o → lld → imagebld → XBE` link pipeline, and the isa-scan gate on real hardware.
- **Phase B — net8+ via the Mono interpreter.** Classic Mono can't host the net8 BCL (different
  runtime+corelib pair), so this means **rebasing the port onto modern Mono** (`dotnet/runtime`
  `src/mono`) and running its **interpreter** — PIII-safe because the interp is C compiled by our
  clang (no SSE2 codegen; see §4). Same lineage as classic Mono, so the PAL hooks, GC OS-bindings,
  toolchain, and link pipeline built in Phase A **transfer**. Cost is perf (interpreter) + memory
  (net8 corelib) + confirming modern Mono still builds for 32-bit x86.

The spike below still runs — it's now a **confirmation** of the x87 assumption (green-lights
Phase A), not an open Mono-vs-NativeAOT choice. The remaining live sub-decision is classic-Mono
**AOT vs mini-JIT vs AOT+interp** (Xbox has no NX, so JIT is legal).

**Spike result (source-level, 2026-09-27) — GREEN.** `vendor/mono/mono/mini/mini-x86.c` emits
**x87 by default**: `OP_FADD/FSUB/FMUL/FDIV` (≈:3636) and all FP load/store/const use the x87
stack (`x86_fld/fst/fldz/fld1/fldcw`). **SSE2 is an opt-in optimization gated on CPU detection**
(`if (mono_hwcap_x86_has_sse2) opts |= MONO_OPT_SSE2; else *exclude_mask |= MONO_OPT_SSE2;`
≈:879); every `movsd`/SSE path is under `cfg->opt & MONO_OPT_SSE2`. A Pentium III reports no SSE2
via CPUID → `MONO_OPT_SSE2` excluded → **Mono auto-selects x87**. Classic Mono was designed to
run on non-SSE2 x86. Still to verify when building: AOT reuses this codegen (expected clean); and
a real `isa-scan` of built output is the authoritative gate.

Gate criteria:

1. **ISA gate** — can the candidate produce a non-trivial FP-using method whose emitted code
   is isa-scan-clean (PIII)? (Mono/x87: expected yes. NativeAOT: expected no.)
2. **Footprint gate** — does a minimal "print a string + do integer + FP math" image plausibly
   fit alongside a GC heap in 64 MB?
3. **PAL effort gate** — rough LOC/complexity estimate of that runtime's OS-hook surface.
4. **Toolchain gate** — can we drive its cross-AOT/compile to `i686-pc-windows-gnu` object
   files that link with lld into an XBE?

---

## 8. Phased milestones

**Phase 0 — Spikes & the gate (research).**
- Reuse `isa-scan.py`; stand up a CI that runs it on any produced object.
- Mono spike: build the Mono x86 backend cross-targeting i686; confirm x87 codegen on an FP
  method; measure isa-scan cleanliness.
- NativeAOT spike: emit ILC output for one method; run isa-scan; quantify the SSE2 hits.
- PAL skeleton: stub the §3 surface (memory/threads/sync/time/debug) as a static lib linking
  against `libkernel` + `libc`.
- **Exit:** the §7 decision, recorded in `docs/decision-runtime.md`.

**Phase 1 — Boot a managed entrypoint (no GC, no threads).** → detailed build plan in
[`phase1-mono.md`](phase1-mono.md) (source subset, config header, PAL-over-libxapi, milestone ladder).
- Minimal runtime init on the `PsCreateSystemThreadEx` main thread (mirror RXDK's EH thread
  rule); PAL memory + debug-print wired.
- Execute one AOT'd (or JIT'd) static method that calls `DbgPrint` via P/Invoke → "hello from
  managed code" on the debug monitor.
- **Exit:** managed IL runs on real HW; isa-scan clean; boots in xemu **and** on a kit.

**Phase 2 — GC + minimal corlib.**
- Bind the GC's OS hooks to `Nt*VirtualMemory` (reserve/commit) + `NtProtectVirtualMemory`
  (write barriers). **Cooperative-mode GC.**
- Real TLS (close the §3 gap) for the current-thread/GC state.
- Trim corlib to the smallest bootable set (String, primitives, Array, basic collections,
  Object, Type shell). Track image size against the 64 MB budget continuously.
- **Exit:** allocate managed objects, trigger a GC, survive; `new`/arrays/strings work.

**Phase 3 — Threads & exceptions.**
- Managed threads on `PsCreateSystemThreadEx` (reuse `threads.c` patterns); monitors/locks on
  `Ke*`/critical sections.
- Managed exception handling mapped onto the runtime's model (Mono uses its own unwinder; keep
  it off the DWARF path unless it's already integrated). Validate `try/catch/finally`.
- **Exit:** multithreaded managed code + working EH on HW.

**Phase 4 — I/O & a usable BCL slice.**
- File I/O via `Nt*File` (read from the DVD/HDD); `Console`-style output via `DbgPrint` and/or
  a framebuffer.
- Decide the reflection/generics story per the chosen runtime (AOT limits vs JIT freedom).
- **Exit:** a managed sample that reads a file, does real work, prints results.

**Phase 5 — Samples & (optional) graphics interop.**
- A small managed sample suite (mirror RXDK-Samples spirit).
- *Optional, later:* P/Invoke thunks to `libd3d8`/`libdsound` for managed access to NV2A/APU —
  explicitly out of scope until the core runtime is solid.

---

## 9. Risk register

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| R1 | **SSE2 codegen** faults on HW | **Critical** | Mono/x87 path; isa-scan CI gate; §4 |
| R2 | corlib + GC heap don't fit in 64 MB | High | Aggressive corlib trimming; measure every phase; devkit (128 MB) only for dev |
| R3 | TLS works but the kernel-only path is a global-lock O(n) table — too slow for the runtime hot path | Medium | **Use `libxapi` real TLS (`TlsAlloc`-style) via RXDK-SDK** (§1a), or implement TEB-/`TlsDataSize`-backed O(1) per-thread storage |
| R4 | GC thread suspension on a kernel that won't hand back arbitrary thread contexts | High | Cooperative-mode GC + safepoints |
| R5 | x87 80-bit vs IEEE-754 `double` semantics (.NET requires strict IEEE) | Medium | Control x87 precision/rounding; test FP conformance; document divergence |
| R6 | Cross-AOT toolchain: driving ILC/mono-aot to `i686-pc-windows-gnu` objs that lld links | Medium | Phase-0 spike proves the pipeline end-to-end before committing |
| R7 | Reflection/generics coverage under AOT | Medium | Prefer Mono JIT (no NX) or AOT+interp fallback |
| R8 | Exception-model mismatch (managed EH vs DWARF vs SEH) | Medium | Use the runtime's own unwinder; reuse RXDK's "run on Ps thread" rule |
| R9 | Upstream divergence / maintenance of a patched runtime | Medium | Keep PAL/patches minimal and in-tree; pin the upstream commit |

---

## 10. Open questions (resolve during Phase 0)

- **Which .NET / which Mono? → DONE: classic `mono/mono`**, forked to **`EqUiNoX-Labs/mono`**,
  branch **`xbox`** (@ `0f53e9e`), added as submodule **`vendor/mono`**. Chosen for its mature x87
  x86 backend (`mono/mini/mini-x86.c`, `MONO_ARCH_USE_FPSTACK`) — the reason Mono can clear the
  SSE2 wall (§4). **First action on the fork:** the Phase-0 x87 spike — build the x86 backend,
  compile one FP method, `isa-scan` the output for x87-only (no `movsd xmm`).
- **Future fork (Phase B only): `dotnet/runtime` → `EqUiNoX-Labs/runtime`** for the net8
  interpreter path — not needed yet. RXDK-SDK is *consumed*, not forked (submodule
  `Team-Resurgent/RXDK-SDK`, no modifications).
- **corlib source** to trim, and how it's built for the target.
- **AOT image → XBE pipeline:** exact command path from IL → i686 `.o` → lld → `imagebld` XBE.
  Does the runtime image link as ordinary objects the RXDK toolchain already handles?
- **JIT vs AOT for Mono** given no-NX makes JIT legal — measure both in Phase 0.
- **Debug/telemetry** during bring-up: `DbgPrint` first; framebuffer text later.

---

## 11. Foundation decision — build on RXDK-SDK (revised, supersedes the §1/§4 "kernel-only" framing)

**Revised from the original "kernel-imports only" framing.** We build the title against the
**RXDK-SDK** (`D:\Git\RXDK-SDK`) — the consumer SDK: prebuilt `.lib`s + headers produced by the
RXDK-Libs build (`libkernel`, `libc`, `libcpp`, **`libxapi`**, `libd3d8`, `libd3dx8`,
`libxgraphics`, `libdsound`, `libxnet`, `libxmv`). This is a deliberate trade:

- **Runtime PAL** still *prefers* kernel imports (`Nt*/Ke*/Mm*/Ps*`) for memory / threads / sync
  / timing — the lean core stays lean.
- **Everything a demo needs off the critical path** — input, audio, graphics, and crucially
  **TLS** — comes from `libxapi` rather than being rebuilt. `libxapi` sets up the real thread
  environment and provides `TlsAlloc`-style TLS, which is a **cleaner fix for R3** than the
  kernel-only emulated table (see §3). We are no longer bound to "kernel imports only" where the
  SDK already solved something well.

Submodules are fine (picolibc / llvm-project / and RXDK-SDK or RXDK-Libs as a submodule/pinned
release) — pin them for reproducible builds.

**CI/release** is **appropriated from RXDK-Tools** (`.github/workflows/build-rxdktools.yml`): the
tag→versioned / push-to-main→moving-`latest` / dispatch→prerelease logic via
`softprops/action-gh-release`, plus a multi-OS build matrix. Our CI adds the **isa-scan gate**
(RXDK-Libs `tools/isa-scan.py`) as a hard PIII-clean check on any native output. See
`.github/workflows/build.yml`.

## 12. Example workload & input

The demonstrator is an **interactive** managed app (keyboard/controller). With RXDK-SDK linked,
**input is no longer a self-build problem**: `libxapi` provides XInput (controllers) and keyboard
(`xkbd.h` / `conio.h`), so the managed side just needs a thin **P/Invoke surface** onto those
exports. (The earlier "assemble a USB-HID stack ourselves over OHCI" plan is dropped — only
revisit it if we ever want a truly xAPI-free build.) On-screen text can likewise start via
`DbgPrint`, then move to a framebuffer/`libxgraphics` path when wanted.

## 13. Immediate next steps

1. ✅ `git init`; vendored `tools/isa-scan.py`; `.gitignore`; CI skeleton appropriated from
   RXDK-Tools (`.github/workflows/build.yml`) with the isa-scan gate wired in.
2. **Wire the foundation:** add RXDK-SDK (or RXDK-Libs) + `picolibc` / `llvm-project` as pinned
   submodules; confirm RXDK-SDK already ships the `.lib`s + headers we link (it does —
   `D:\Git\RXDK-SDK\{lib,include}`, and `build-out/lib` in RXDK-Libs is populated).
3. ✅ **Title-link pipeline proven with C (2026-09-27).** `tests/hello-c` builds via the `rxdk`
   engine (clang `i686-pc-windows-gnu -march=pentium3` → lld `-e XapiTitleStartup` → `imagebld`
   XBE → `xdvdfs` ISO) and **boots on xemu**, printing over the LPC47M157 UART
   (`xemu -dvd_path <iso> -device lpc47m157 -serial stdio`). Toolchain staged at
   `C:\ProgramData\RXDK\{llvm,sdk,tools}`; xemu-devkit at `D:\Git\xemu-devkit`. The
   `IL→.o→lld→imagebld` path is de-risked.
4. Run the **two Phase-0 spikes** (Mono x87 codegen; NativeAOT ISA hits); isa-scan both.
5. Write `docs/decision-runtime.md` and pass the §7 gate.
