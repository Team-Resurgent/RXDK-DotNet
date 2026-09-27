# Phase-1 link surface — trial-link of the Mono runtime (2026-09-27)

`scripts/build-host.sh` links the four archives (`libmini` + `libmonoruntime` + `libmonoutils` +
`libeglib`) + the PAL glue (`win32_supplement.o`, `win_crt_compat.o`) + a minimal embedding host
against RXDK-SDK (`libxapi`/`libkernel`/`libc`/`libcpp`/`libcompat`) + compiler-rt builtins, entry
`XapiTitleStartup`. With `--allow-multiple-definition` (the SDK's `xtl.h` D3DX math inlines get
emitted in every TU — dead code Mono never calls), the link resolves everything **except 276
undefined symbols** — the true remaining PAL/runtime surface. It groups cleanly:

## The surface (276 symbols)

| Bucket | Count | Resolution |
|---|---|---|
| **Win32 wide (W) variants** — `CreateEventW`, `CreateMutexW`, `OpenEventW/MutexW/SemaphoreW`, `GetVersionExW`, `GetEnvironmentVariableW`, … | 10 | W→A thunks over `libxapi` (the SDK ships A variants only — the UNICODE-vs-Win2000 gap again) |
| **Interlocked 64-bit / pointer** — `InterlockedCompareExchange64`, `InterlockedExchangePointer`, `InterlockedAdd64`, … | 8 | implement in `win32_supplement.c` over `__sync_*` intrinsics (libxapi has 32-bit only) |
| **Dynamic loading / misc Win32** — `LoadLibrary(W/ExW)`, `GetProcAddress`, `FreeLibrary`, `GetTickCount(64)`, `NtCurrentTeb`, `AddVectoredExceptionHandler`, env-strings, `Sleep`, `InitializeCriticalSectionEx`, `TryAcquireSRWLockExclusive` | ~25 | PAL shims; dynamic-loading ones stub (no DLLs on Xbox → static P/Invoke) |
| **CRT gaps** — `_access`, `_utime`, `_ftime`, `_wmkdir`, `_fileno`, `_ecvt_s`, `__writefsdword` | ~8 | forward to picolibc / small shims in `win_crt_compat.c` |
| **eglib win32 file/dir** — `monoeg_g_dir_open/close/read_name`, `g_file_get_contents`, `g_file_test`, `g_get_current_dir`, `g_mkdtemp` | 7 | **un-defer** the eglib `gdir-win32`/`gfile-win32`/`gspawn` files (fix `_O_*` flags + the SDK `WIN32_FIND_DATA` bug) |
| **zlib** — `inflate`, `inflateInit2_` | 2 | Mono decompresses metadata; provide a minimal zlib (or the SDK's) or disable compressed-image support |
| **BCL icalls** (`ves_icall_*`) — Net.Sockets (28), Globalization (10), System.IO (7), Security.Cryptography (5), … | 71 | mostly from excluded subsystems (sockets/security/locale); stub to throw, or disable the corresponding BCL features |
| **Mono internal from excluded files** — `mini_llvmonly_*` (llvmonly-runtime), `mono_aot_*` (aot-runtime), regalloc (`mono_alloc_ireg/freg`) | ~125 | stub — the interpreter-only path references these symbolically but does not call them at runtime |

## What this tells us

- The **compile** phase is complete and correct: no eglib/utils/sgen/metadata/mini symbols are
  missing between the archives — they resolve against each other. The 276 gaps are all either
  **external** (Win32/CRT/zlib the SDK doesn't provide) or **deliberately-excluded** files
  (llvm/aot/sockets/security) that the interp path references but doesn't execute.
- No sign of a fundamental blocker: it's a finite shim/stub list, not a redesign.

## Resolution plan (Phase-1 endgame, continued)

1. **Grow `win32_supplement.c`**: W→A Win32 thunks, Interlocked64/pointer over `__sync`, tick/env/
   TEB/VEH shims, dynamic-loading stubs.
2. **A `mono_stubs.c`**: the `mini_llvmonly_*` / `mono_aot_*` / unused-icall symbols as
   abort-or-throw stubs (interp-only never calls them).
3. **Un-defer the eglib win32 file/dir files** (they're needed for assembly loading) — fix `_O_*`
   and the RXDK-SDK `WIN32_FIND_DATA`/UNICODE bug (upstream the SDK fix).
4. **zlib**: minimal decompressor or disable compressed metadata.
5. Iterate the link to zero undefined → an `.xbe` that *links*.
6. **Then the runtime dependency the link doesn't show: corlib.** Mono needs its managed class
   library (`mscorlib`/`System.Private.CoreLib` equivalent — mono's `mcs/class/corlib`) present at
   boot or `mono_jit_init` fails. Building/trimming corlib for the target is its own milestone
   (Phase 1b) and is the gate to actually *running* a managed method.

## Result (2026-09-27): 0 undefined → the runtime LINKS, BOOTS, and RUNS Mono init

The 276 symbols resolved to **0** — `mono-host.exe` (~3.3 MB PE) links, `imagebld` → `.xbe`,
`xdvdfs` → `.iso`. **Booted on xemu, and Mono's own runtime code executes on real-Xbox HLE:**

```
RXDK.start: main
RXDK-DotNet: mono embedding host starting     <- our embedding host
DECL_OFFSET2(CallContext,stack,20) ...        <- Mono runtime executing (mini-cross-helpers)
#endif //disable jit check
lldb support has been disabled at configure time.
```

So the linked runtime boots and runs Mono init far past "it links". It then stops inside
`mono_jit_init` — it takes the **cross-offsets dump path** (`mini-cross-helpers.c`), which suggests
`MONO_CROSS_COMPILE` is effectively engaged, and corlib is absent. Two next items:
1. **Fix the offsets-dump path** — ensure the target runtime build doesn't take the cross-compiler
   offsets branch in `mini_init` (config/`MONO_CROSS_COMPILE`).
2. **corlib (Phase 1b)** — the managed BCL so `mono_jit_init` completes and a method can run.

`scripts/build-host.sh` now also packages the XBE + ISO, so the bootable image is reproducible.
