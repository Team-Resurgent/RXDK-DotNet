# Phase 1 — cross-building the Mono runtime for the original Xbox

Concrete build plan for standing up `libmono` on the Xbox target. Synthesised from a survey of
`vendor/mono` (Mono **6.13.0**, branch `xbox`) + the RXDK build mechanics.

## Strategy in one paragraph

**Bypass Mono's autotools entirely** and compile a hand-picked subset of Mono's `.c` files with
the RXDK clang/lld toolchain via an engine build manifest (`build/sdk/libmono.json`, mirroring how
`libcpp.json` compiles vendored libc++). Use a **hand-written config header** (fork of Mono's
`winconfig.h`) instead of an autogen'd `config.h`. Build as **`HOST_WIN32` + `TARGET_X86`** and
**reuse Mono's existing Win32 PAL**, backed by RXDK-SDK's `libxapi` (which already exports
`CreateThread`/`VirtualAlloc`/`VirtualProtect`/`TlsAlloc`/`WaitForSingleObject`/`CRITICAL_SECTION`/…).
Ship **interpreter-first** (`DISABLE_JIT`) for the most robust "managed code runs" milestone —
managed arithmetic then executes in the interpreter's C, compiled x87-clean by our `-march=pentium3`
clang, sidestepping all JIT codegen risk. The x87 JIT (spike-green) is a later optimisation.

## Why interp-first (recap + caveat)

- Robust bring-up: no managed-method codegen at all → no SSE2 risk for managed code, and it's the
  same engine a later runtime on this CPU would use.
- **Caveat (survey finding): the interpreter is not standalone.** `interp.c` calls
  `mono_arch_get_interp_to_native_trampoline`, `mono_arch_{set,get}_native_call_context_*`, and
  `mono_jit_compile_method_jit_only` (marshaling wrappers). So an interp-only build **still links
  `libmini`** (`mini-runtime.c`, `driver.c`, exceptions) **and the x86 arch layer** (`tramp-x86.c`,
  parts of `mini-x86.c`, `exceptions-x86.c`, `mono-context.c`). These are hand-written
  calling-convention/trampoline/context code — **audit them for SSE2** (PIII = SSE1 only). The
  managed FP path is safe; this arch glue is the thing to verify.

## Config header (`build/generated/mono/config.h` + `eglib-config.h`)

Fork `vendor/mono/winconfig.h` (repo-root template) and `vendor/mono/mono/eglib/eglib-config.hw`.
Key defines:

- **Arch/host:** `HOST_WIN32=1`, `TARGET_WIN32=1`, `TARGET_X86=1`.
- **GC:** `HAVE_SGEN_GC=1`; leave `HAVE_BOEHM_GC` unset.
- **Keep on:** `ENABLE_ILGEN=1` (required whenever interp is on), interpreter (leave
  `DISABLE_INTERPRETER` **unset**).
- **Turn off** (via the `--enable-minimal` define set): `DISABLE_JIT` (interp-only),
  `DISABLE_COM`, `DISABLE_REMOTING`, `DISABLE_APPDOMAINS`, `DISABLE_REFLECTION_EMIT_SAVE`,
  `DISABLE_PROCESSES`, `DISABLE_SOCKETS`, `DISABLE_PROFILER`, `DISABLE_AOT`, `DISABLE_ATTACH`,
  `DISABLE_PORTABILITY`, `DISABLE_CRASH_REPORTING`. (`winconfig.h` already sets several of these.)
- eglib: start from `eglib-config.hw` (the Windows variant).

Authoritative option list: `configure.ac` `--enable-minimal` help (~:1759) + the `AC_DEFINE`
blocks (~:1759–1912). No `config.h.in` exists in this tree — `winconfig.h` is the intended template.

## Source compile set (engine only — from `msvc/*.vcxproj`)

Derive the exact file list from the MSVC projects (they enumerate sources explicitly), not the
Makefiles:

- `msvc/libmonoutils.vcxproj`  → `mono/utils/*` (pick `*-windows.c` / `HOST_WIN32` variants)
- `msvc/libgcmonosgen.vcxproj` → `mono/sgen/*.c`
- `msvc/libmonoruntime.vcxproj`→ `mono/metadata/*.c` (minus `w32socket*`/`w32process*` and
  disabled-feature files)
- `msvc/libmini.vcxproj`       → `mono/mini` `common_sources` + `x86_sources`
  (`mini-x86.c`, `tramp-x86.c`, `exceptions-x86.c`) + `windows_sources`
  (**audit/replace `mini-windows-dllmain.c`** — DLL semantics don't apply to a static XBE)
- `msvc/libmono-ee-interp.*`   → `mono/mini/interp/*.c`
- `mono/eglib/*.c` (drop GModule/dl if unused)
- headers: `mono/arch/x86/x86-codegen.h`, plus `mono/native/*` PAL helpers if referenced

Layer order for linking mirrors RXDK convention (engine handles it): eglib → utils → sgen →
metadata → mini + interp, over `libxapi`/`libc`/`libcpp`/`libkernel`.

## PAL: reuse Mono's `HOST_WIN32` path over `libxapi`

Selection is one knob (`HOST_WIN32`, `configure.ac:566`) picking `*-windows.c` files +
in-source `#if HOST_WIN32`. The Win32 backend files already exist:
`mono-threads-windows.c`, `mono-mmap-windows.c`, `mono-os-semaphore-win32.c`,
`mono-os-wait-win32.c`, `os-event-win32.c`, `mono-dl-windows.c`, `w32*-win32.c`,
`mini-windows*.c`. Our job is to **satisfy the Win32 APIs they call** — nearly all present in
`libxapi` (verified). Gaps / watch items:

- **`LoadLibraryA` / `GetProcAddress`** — absent on Xbox (no dynamic loading). Disable dllmap /
  use static P/Invoke; stub these two if still referenced.
- **`w32socket` / `w32process`** — pull a large sockets/process surface; exclude the files and
  `DISABLE_SOCKETS`/`DISABLE_PROCESSES`.
- **`mini-windows-dllmain.c` / TLS-callback bootstrap** — replace with an explicit runtime-init
  call (we control startup; there is no DllMain in a static XBE).
- **`VirtualAlloc`/`VirtualProtect`/`VirtualFree`** — must honour reserve-vs-commit and W^X
  (SGen and the interp's code-manager depend on it). `libxapi` provides them; verify semantics
  against `Nt*VirtualMemory`.

## Freestanding hazards (survey) + mitigations

| Hazard | Reality | Mitigation |
|---|---|---|
| `fork()` | 0 uses in core | none needed |
| `execv` | only in `w32process`/`mono-proclib` | `DISABLE_PROCESSES`, don't compile those |
| `sigaction`/signals | POSIX path only (`mini-posix.c`); Win32 uses SEH/vectored | interp-only + `DISABLE_CRASH_REPORTING`; stub exception/signal wiring |
| `dlopen` | POSIX path; Win32 uses `LoadLibrary` | disable dllmap / static pinvoke |
| `mmap` | routed through `VirtualAlloc` on Win32 | supply from `libxapi` |
| **SSE on x86** | JIT `mini-x86.c` uses SSE arg classes; **interp bypasses managed codegen** | interp-only; **audit `mono-context.c`/`mini-x86.h`/`tramp-x86.c` for SSE2** (PIII=SSE1) |
| env/`getenv`, GAC, config | hosted assumptions | `DISABLE_CONFIG`/`DISABLE_GAC`; feed assemblies explicitly |
| assembly file IO | `w32file` / `fopen` | back with picolibc file IO (we have `Nt*File`) |

## Phase-1 milestone ladder

1. **eglib compiles** — smallest unit; proves config header + flags. `libeglib.lib`.
2. **utils compiles** (HOST_WIN32 variants) — proves the PAL file selection + first missing-symbol
   list against `libxapi`. Resolve/stub gaps.
3. **Whole engine compiles** to `libmono.lib` (compile-only; expect an undefined-symbol list — the
   Win32 APIs + a few stubs). Close them. Wire `tools/isa-scan.py` over the objects (must be
   PIII-clean).
4. **Minimal embedding host** — a tiny C `main` that calls `mono_jit_init` (or interp init) +
   `mono_runtime_exec_main` on a trivial IL assembly, linked into an XBE via the proven
   `hello-c` pipeline.
5. **Bundle a trivial managed assembly** (a C# `Main` that writes a line) into the ISO; runtime
   loads + interprets it; **see managed output on the xemu serial UART** — the Phase-1 "hello,
   managed world" milestone.

## Milestone 1 — status (2026-09-27): eglib compiling ✅

`scripts/build-eglib.sh` compiles **24/28** eglib sources with the RXDK clang and archives
`build-out/lib/libeglib.lib`. This proved the config + toolchain + compat approach:

- **Config headers authored:** `build/generated/mono/config.h` (clang-gnu adaptation of
  `winconfig.h`: `HOST_WIN32`+`TARGET_X86`+SGen, `DISABLE_JIT`, feature disables, sizes, `__thread`,
  and a `__forceinline` define for the SDK's Windows headers) and
  `build/generated/mono/eglib-config.h` (the `eglib-config.h.in` template resolved for i686 Win32).
- **MSVCRT compat shims (new, in `pal/` + `build/generated/compat/`):** `win_crt_compat.h/.c`
  (`_read`/`_write`/`_open`/`_close`/`_lseek`/`_unlink`/`_mktemp` → POSIX forwarders — RXDK libc has
  the string/printf underscore names but not these file-IO ones), plus minimal `direct.h` and `io.h`
  shims. TODO: upstream the file-IO names into RXDK-Libs `ms_crt_compat.c`.
- **Compile flags that matter:** `-target i686-pc-windows-gnu -march=pentium3 -ffreestanding
  -femulated-tls`, `-DHAVE_CONFIG_H`, force-include `config.h` + `win_crt_compat.h`, and
  **`-fms-extensions -fms-compatibility`** — required for any TU that includes `windows.h`/`xtl.h`
  (clears the `__forceinline`/`_inline` MSVC-keyword cascade from `d3d8.h`/`d3dx8math.inl`).
- **Archive gotcha:** MSYS mangles `llvm-lib /OUT:`; the script uses `cygpath -w` +
  `MSYS2_ARG_CONV_EXCL='*'`.

**Deferred to milestone 1b** (win32 backends, not needed for interp bring-up): `gspawn`,
`gdate-win32`, `gdir-win32` need a `winsock2.h` stub (sockets are disabled anyway); `gfile-win32`
needs `_O_BINARY`/`_O_CREAT` defines + a wide-char decl fix. `gfile-win32` will be wanted once
assembly file-loading is wired.

## Milestone 2 — status (2026-09-27): mono/utils compiling ✅

`scripts/build-utils.sh` compiles **77/77 relevant `mono/utils` sources** (0 failures) and archives
`build-out/lib/libmonoutils.lib`. **This validates the core PAL strategy:** every gap was a missing
Win32 *type/constant*, never a missing OS *function* — Mono's threading/mmap/sync/TLS APIs all
resolve against RXDK-SDK's `libxapi`. The Win2000-era Xbox surface just lacks the modern (Vista+)
Win32 declarations Mono 6.13 assumes.

- **`USE_GCC_ATOMIC_OPS`** added to `config.h` → `mono-membar.h` uses `__sync_synchronize()`.
- **`pal/include/rxdk/win32_supplement.h`** (new, force-included): the Win32 types/constants the SDK
  omits — `LONG64`/`UINT64`, `SRWLOCK`, `CONDITION_VARIABLE`, `SYSTEM_INFO`+`GetSystemInfo`,
  `MEMORYSTATUSEX`+`GlobalMemoryStatusEx`, `NT_TIB`, `PROCESSOR_NUMBER`, `WSAEVENT`,
  `TLS_MINIMUM_AVAILABLE`, `DUPLICATE_SAME_ACCESS`, `MAXIMUM_WAIT_OBJECTS`,
  `HEAP_CREATE_ENABLE_EXECUTE`, `CRITICAL_SECTION_NO_DEBUG_INFO`, `MEMORY_ALLOCATION_ALIGNMENT`.
- **Header shims:** `winsock2.h` (defensive include → pulls the supplement), `process.h`
  (`_beginthreadex`), `psapi.h` (`GetProcessMemoryInfo`).
- **Excluded** (not our target / disabled subsystems): non-x86 hwcap (arm/riscv/s390x/sparc),
  other-platform threads (mach/wasm/posix-signals), networking, processes/psapi (proclib),
  bcrypt-rand, dlmalloc, dynamic-loading (mono-dl), io-portability.

**Owed to link time — `win32_supplement.c` (TODO, the real PAL emulation):** the supplement only
*declares* the Vista-only primitives; they need definitions over what the Xbox provides —
`SRWLOCK`/`CONDITION_VARIABLE` over `CRITICAL_SECTION`+events, `GlobalMemoryStatusEx` over
`MmQueryStatistics`, `GetSystemInfo` over the kernel, `_beginthreadex` over `CreateThread`. These
are the genuine PAL shims (small). Compilation doesn't need them; the eventual runtime link does.

## Milestone 3 — status (2026-09-27): sgen + metadata compiling ✅

`scripts/build-metadata.sh` compiles **132/132** `mono/sgen` + `mono/metadata` sources (0 failures)
→ `build-out/lib/libmonoruntime.lib`. utils re-verified at 77/77, eglib at 26/28 — no regressions.
The GC + assembly loader + type system now build for the Xbox.

Key changes this milestone:
- **Dropped `-fms-compatibility` (kept `-fms-extensions`) across all build scripts.** Critical
  lesson: `-fms-compatibility` makes clang define *neither* `__GNUC__` nor `_MSC_VER`, so Mono's
  GCC-path code (`sgen_dummy_use`, `mono-membar.h`, …) hit `#error`. `-fms-extensions` alone keeps
  `__GNUC__` and still accepts the MSVC keywords; the two spellings the SDK's D3D headers use
  (`__forceinline`, `_inline`) are `#define`d in `config.h`.
- **`config.h`:** `USE_WINDOWS_BACKEND` (adds `windows_tib` to `MonoThreadInfo`),
  `UNICODE`/`_UNICODE` (generic Win32 A/W macros → W, as Mono expects), `USE_GCC_ATOMIC_OPS`.
- **`win32_supplement.h` grew** to cover everything metadata references: `WSABUF`, `WIN32_FIND_DATAW`,
  `OSVERSIONINFO(EX)` A/W, `IMAGE_DOS_HEADER`, `struct _timeb`/`utimbuf`, `COINIT_*`,
  `FILE_ATTRIBUTE_*`/`FILE_TYPE_*`, `REPLACEFILE_*`.
- **New header shims:** `objbase.h` (COM include), `process.h`, `psapi.h`; `winsock2.h` now also
  pulls the SDK's real `sys/_timeval.h` for `struct timeval`.
- **Excluded** (disabled subsystems / not needed): COM (`coree`, `cominterop`, `marshal-windows`),
  sockets (`w32socket*`, `threadpool-io*`), processes (`w32process*`), `mono-security-windows`,
  `console-null`.

**Deferred straggler — `w32file-win32.c` (1 file):** blocked by a genuine **RXDK-SDK bug** —
`winbase.h:1447` does `typedef WIN32_FIND_DATAA WIN32_FIND_DATA` *unconditionally* (ignores
`UNICODE`) and ships no `WIN32_FIND_DATAW`, so Mono's generic find_first decl clashes with its
explicit-W definition. **Fix belongs upstream in RXDK-SDK** (make `winbase.h` UNICODE-aware + add
the W struct). Needed for Win32 file IO at milestone 4.

**Still owed at link time:** `win32_supplement.c` — definitions for the Vista-only primitives the
supplement declares (`SRWLOCK`/`CONDITION_VARIABLE` over `CRITICAL_SECTION`, `GlobalMemoryStatusEx`
over `MmQueryStatistics`, `_beginthreadex` over `CreateThread`, `GetSystemInfo`, …).

## Milestone 4 — status (2026-09-27): mini + interp compiling, SSE1 audit PASSED ✅

`scripts/build-mini.sh` compiles **58/58** `mono/mini` + `mono/mini/interp` sources (0 failures) →
`build-out/lib/libmini.lib`. **The entire Mono runtime engine now compiles for the Xbox** —
eglib + utils + sgen + metadata + mini + interp, ~5.6 MB across four archives.

**SSE1 codegen audit — the §4 concern — RESOLVED.** `python tools/isa-scan.py build-out/lib` is
**clean: no instruction above SSE1** across all four archives, including the arch glue that
interp-only doesn't insulate (`tramp-x86.o`, `mini-x86.o`, `mono-context.o`) and the interpreter
(`transform.o`, `interp.o`). The core bet — that Mono on `-march=pentium3` clang stays PIII-safe —
is now proven in emitted object code, not just at the source level.

Key changes this milestone:
- **`HAVE_SGEN_GC` moved from `config.h` to a per-batch `-D` flag** (utils/sgen/metadata get it;
  eglib/mini do not). `mono/mini/mini.h` deliberately `#error`s if it sees the GC define, so the
  same mini objects can link into either runtime.
- **Generated header:** `cpu-x86.h` produced via `genmdesc.py TARGET_X86 … cpu-x86.md` (Mono's
  machine-description compiler) into `build/generated/mono/`. Minimal `version.h` provided too.
- **`win32_supplement.h`:** SEH filter constants (`EXCEPTION_CONTINUE_SEARCH`, …), DLL entry
  reasons (`DLL_PROCESS_ATTACH`, …), `PIMAGE_TLS_CALLBACK`.
- **Excluded** (embedded runtime / not our target): all non-x86 arches (amd64/arm/arm64/ppc/mips/
  s390x/sparc/riscv/wasm/loongarch64), LLVM, the AOT compiler, `main`/`main-sgen` (the `mono.exe`
  launcher — we write our own embedding host), `debugger-agent` (socket soft-debugger),
  `mini-windows-dllmain`/`-dlldac`/`-tls-callback` (DllMain/DAC/TLS-callback → replaced by explicit init).

## Phase-1 endgame — status (2026-09-27): PAL glue written, link surface enumerated

`win32_supplement.c` (the Vista-primitive emulation) + `win_crt_compat.c` compile, and the trial
link of all four archives + PAL + a minimal embedding host against RXDK-SDK enumerates the full
remaining surface: **276 undefined symbols, all external or from deliberately-excluded files** —
no missing inter-archive symbols, i.e. the compile phase is coherent. Full categorized breakdown +
resolution plan in [`phase1-link-surface.md`](phase1-link-surface.md). Headlines: Win32 wide-variant
thunks + Interlocked64 (PAL), `mini_llvmonly_*`/`mono_aot_*`/unused-icall stubs, un-defer the eglib
win32 file/dir files, a minimal zlib — then **corlib** (the managed BCL) is the runtime gate to
actually executing a method.

Config fix this stage: `_inline` → `static __inline` (the SDK's `d3dx8math.inl` helpers must have
internal linkage), and the link uses `--allow-multiple-definition` for the residual `__forceinline`
header inlines (dead D3DX code).

## Phase-1 endgame (remaining)

The compile phase is done; the link surface is known. Remaining to a booting managed "hello":
1. **`win32_supplement.c`** — implement the Vista-only primitives the supplement declares
   (`SRWLOCK`/`CONDITION_VARIABLE` over `CRITICAL_SECTION`+events, `GlobalMemoryStatusEx` over
   `MmQueryStatistics`, `GetSystemInfo`, `_beginthreadex` over `CreateThread`, …) + the MSVCRT
   file-IO forwarders.
2. **Link `libmono`** (the four archives) + `win32_supplement.o` + a tiny **embedding host**
   (`mono_jit_init` / interp init → run a method) into an `.xbe` via the proven `hello-c` pipeline
   (RXDK engine). Resolve the undefined-symbol tail against `libxapi`/`libc`.
3. **Bundle a trivial managed assembly** (C# `Main` writing a line) into the ISO; interpret it;
   **managed output on the xemu serial UART** — the Phase-1 payoff.
4. Fold back the deferred files once their blockers clear: `w32file-win32` (RXDK-SDK
   `WIN32_FIND_DATA`/UNICODE fix), `gfile-win32` (`_O_*` flags).

## First files to open

`msvc/libmini.vcxproj` + `msvc/libmonoruntime.vcxproj` (source lists), `winconfig.h`,
`mono/eglib/eglib-config.hw`, `mono/mini/mini-runtime.c` (init path → `mono_ee_interp_init`),
`mono/mini/interp/interp.c`, `mono/utils/mono-mmap-windows.c`, `mono/utils/mono-threads-windows.c`,
`mono/mini/mini-x86.c` + `tramp-x86.c` + `mono/mini/mono-context.c` (SSE audit).

## Open risks specific to Phase 1

- **SSE2 in the arch/context glue** (not managed codegen) — the one place interp-only doesn't
  fully insulate us. Audit + isa-scan early.
- **Missing-symbol tail** — the true PAL surface only reveals itself when the engine links; budget
  iteration on stubs.
- **`w32handle` model** — Mono's handle layer may assume more than `libxapi` gives; may need a thin
  shim.
- **Assembly loading** — feeding corlib + the app assembly to a freestanding runtime (from the DVD
  via `Nt*File`) is its own mini-milestone.
