/*
 * RXDK-DotNet — hand-written Mono config.h for the original Xbox target.
 *
 * Mono's autogen can't produce a config.h for our freestanding clang-gnu cross build, and the
 * stock winconfig.h is MSVC-only (its body is under #ifdef _MSC_VER, else it includes cygconfig.h
 * which we don't have). This is a clang-gnu / i686-pc-windows-gnu adaptation of winconfig.h's body.
 *
 * Target: HOST_WIN32 + TARGET_X86 (32-bit), SGen GC, interpreter-first (DISABLE_JIT).
 * See docs/phase1-mono.md. Expect to grow as metadata/mini reveal more required defines.
 */
#pragma once

/* clang on the gnu triple does not predefine MSVC's __forceinline, but the SDK's Windows headers
 * (windows.h -> xtl.h -> d3d8.h, …) use it as a decl-specifier. Provide it. */
#ifndef __forceinline
#define __forceinline __inline__ __attribute__((__always_inline__))
#endif

/* ---- VES / codegen ------------------------------------------------------- */
#define ENABLE_ILGEN 1          /* required whenever the interpreter is enabled */
#define DISABLE_JIT 1           /* interpreter-first bring-up (port-plan.md §4) */
/* Leave DISABLE_INTERPRETER UNSET so the interp EE is compiled in. */

/* ---- disabled subsystems (freestanding console; see phase1-mono.md) ------ */
#define DISABLE_CRASH_REPORTING 1
#define DISABLE_PORTABILITY 1
#define DISABLE_COM 1
#define DISABLE_REMOTING 1
#define DISABLE_REFLECTION_EMIT_SAVE 1
#define DISABLE_PROCESSES 1
#define DISABLE_PROFILER 1
#define DISABLE_ATTACH 1
#define DISABLED_FEATURES "jit,com,remoting,reflection_emit_save,processes,profiler,attach"

/* ---- GC ------------------------------------------------------------------ */
#define HAVE_SGEN_GC 1
#if defined(HAVE_SGEN_GC) && !defined(HAVE_CONC_GC_AS_DEFAULT)
#define HAVE_CONC_GC_AS_DEFAULT 1
#endif
#if defined(HAVE_SGEN_GC) && !defined(HAVE_MOVING_COLLECTOR)
#define HAVE_MOVING_COLLECTOR 1
#endif
#if defined(HAVE_SGEN_GC) && !defined(HAVE_WRITE_BARRIERS)
#define HAVE_WRITE_BARRIERS
#endif

/* ---- host / target ------------------------------------------------------- */
#define HOST_WIN32 1
#define TARGET_WIN32 1
#define TARGET_X86 1
#define HOST_X86 1
#define HOST_NO_SYMLINKS 1
#define MONO_ARCHITECTURE "x86"
#define TARGET_BYTE_ORDER G_BYTE_ORDER
#define TARGET_SIZEOF_VOID_P SIZEOF_VOID_P

/* ---- sizes (i686) -------------------------------------------------------- */
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_REGISTER 4
#define SIZEOF_VOID_P 4

/* ---- TLS / misc ---------------------------------------------------------- */
#define MONO_KEYWORD_THREAD __thread   /* clang -femulated-tls lowers this */
#define MONO_ZERO_LEN_ARRAY 0          /* gcc/clang zero-length array idiom */
#define STDC_HEADERS 1

/* ---- libc feature probes (picolibc-backed via RXDK-SDK) ------------------ */
#define HAVE_ACCESS 1
#define HAVE_COMPLEX_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SIGNAL 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRTOK_R 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_WCHAR_H 1

/* ---- package identity ---------------------------------------------------- */
#define MONO_CORLIB_VERSION "1A5E-RXDK-PLACEHOLDER"  /* TODO: real corlib-interface version */
#define PACKAGE "mono"
#define PACKAGE_NAME "mono"
#define PACKAGE_STRING "mono 6.13.0"
#define PACKAGE_TARNAME "mono"
#define PACKAGE_VERSION "6.13.0"
#define VERSION "6.13.0"

/* Windows builds default to preemptive suspend. */
#undef ENABLE_HYBRID_SUSPEND
