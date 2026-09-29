# Phase 1b — corlib + running a managed method

The runtime links and boots (`docs/phase1-link-surface.md`); `mono_jit_init` runs Mono's init code
on the Xbox but can't complete without the managed base class library. Phase 1b builds
**classic-Mono-6.13 `mscorlib.dll`** for the target, gets `mono_jit_init` to load it, and runs a
trivial managed method to serial — the "hello, managed world" payoff.

## Corlib version — pinned

`config.h` now sets `MONO_CORLIB_VERSION = "1A5E0066-58DC-428A-B21C-0AD6CDAE2789"` (mono 6.13,
`configure.ac:66`). The runtime compares this against `mscorlib`'s `Consts.MonoCorlibVersion` at
load, so **the corlib we build must come from this exact mono tree** (it does — `vendor/mono`), and
**metadata must be rebuilt** so the corrected version is baked in (it was a placeholder before).

## Build approach — Roslyn `/nostdlib` (no mcs bootstrap)

`dotnet` 10.0.400 is present → use Roslyn `csc`. Mono's corlib is a normal C# assembly built
`/nostdlib` (it *defines* the core types), so we don't need to bootstrap `mcs`.

1. **Assemble the source list** from `mcs/class/corlib`:
   base `corlib.dll.sources` (1923) + `net_4_x_corlib.dll.sources` + `win32_net_4_x_corlib.dll.sources`,
   **minus** the matching `*_exclude.sources`. Paths are relative to the corlib dir; several pull
   shared files from `../../build/common/` (`Consts.cs`, `Locale.cs`, `AssemblyRef.cs`, `SR.cs`).
2. **Defines** (from `mcs/build/profiles/net_4_x.make` + `config-default.make`):
   `NET_4_0;NET_4_5;NET_4_6;MONO;MONO_FEATURE_*` (audit the exact `MONO_FEATURE_` set; disable the
   ones matching subsystems we stubbed — sockets/appletls/etc.).
3. **Generated sources:** most `build/common/*` are checked in; watch for any `.cs.in` that need
   substitution.
4. **Compile:** `csc /nostdlib /noconfig /target:library /unsafe /d:<defines> @sources -out:mscorlib.dll`.
   Iterate on the error tail (missing files, define mismatches) — expect several rounds.

## Assembly loading on the Xbox

`mono_jit_init` searches for `mscorlib.dll` on the assembly path. The host must point Mono at the
DVD before init:
- `mono_set_assemblies_path()` / `mono_assembly_setrootdir()` → the ISO dir (`\Device\CdRom0\` or the
  mounted title path), and bundle `mscorlib.dll` (+ the app assembly) into the ISO next to
  `default.xbe`.
- Then `mono_jit_init(...)` → should return non-NULL once corlib loads and its version matches.

## Interpreter mode

Force interp so no JIT/AOT codegen runs (interp-first, §4): set `mono_jit_set_aot_mode` /
`mono_use_interpreter` / `MONO_ENV_OPTIONS=--interpreter` equivalent via the embedding API before
executing the entry method.

## Watch item created by the trial link: stubbed icalls

The link resolved via 187 no-op stubs including **71 `ves_icall_*`**. The **core** icalls (String,
Object, Array, Type, GC, basic MonoIO) are *real* — they come from the compiled metadata files, not
the excluded ones — so basic corlib init should work. The stubs cover **disabled subsystems**
(sockets, crypto, globalization, IO-selector); they only bite if corlib exercises those during init.
As corlib bring-up proceeds, replace any stubbed icall that actually gets called with the real
implementation (un-defer its metadata file, or implement the icall).

## Also fix: the env layer

`g_hasenv("DUMP_CROSS_OFFSETS")` returned true (spurious offsets dump), meaning our
`GetEnvironmentVariableW` path reports found for everything (libxapi's real one likely won the
`--allow-multiple-definition` race). Make env queries reliably "not found" so Mono doesn't misread
`MONO_PATH`/`MONO_DEBUG`/etc. — either ensure our stub wins the link or implement a real empty-env
`GetEnvironmentVariableW`.

## Spike result (2026-09-27) — corlib is buildable with Roslyn

`scripts/build-corlib.sh` assembles the source list (`corlib.dll.sources` + `win32_net_4_x` −
excludes) and compiles with Roslyn `/nostdlib`. Findings:

- **corlib's core `System.*` types live in mono's `external/corefx` submodule** (not initially
  checked out). Initializing it took the build from **41,422 → 1,254 errors** — the `System.Void`/
  `Int32` "predefined type not defined" avalanche was just the missing core files. `build-corlib.sh`
  now inits `external/corefx` (+ `external/referencesource`) automatically.
- **No fundamental incompatibility**: Roslyn 10 compiles mono 6.13 corlib; the remaining 1,254 are
  ordinary iteration — CS0246 (a few more missing sources incl. the generated `Consts.cs` and extra
  externals), CS0308 (`IEnumerable<T>` — a missing generic-collections source set or a `MONO_FEATURE_`
  define), CS0115/0538/0050/0051 (accessibility/partial-type mismatches from source-set gaps).
- Remaining work is source-list completeness + `-define` tuning (audit the exact `net_4_x`
  `MONO_FEATURE_*` set), then link. Multi-iteration but tractable.

## RESULT (2026-09-27): mscorlib.dll BUILDS — 0 errors, 4.7 MB, 1994 sources

`scripts/build-corlib.sh` now produces a classic-Mono-6.13 `mscorlib.dll` with Roslyn. Error
trajectory: **41,422 → 1,254 → 531 → 40 → 5 → 0.** The fixes, in order of impact:
1. **`external/corefx` submodule** (core `System.*` types) — 41k → 1.2k.
2. **Glob-aware source expansion** — mono `.sources` entries are globs with `:excluded` suffixes
   (e.g. `Generic/*.cs:Foo.cs`); a literal read skipped the generic collections. 1.2k → 531.
3. **`external/corert` submodule** (`CancellationToken`, `Stream`, `SafeBuffer`, …) — 531 → 40.
4. **`win32_build_corlib.dll.sources`** (the win32 base list — `Interop.Libraries` etc.) added
   alongside `win32_net_4_x`; path-level exclude subtraction to kill glob/literal dups
   (`RegistryAccessRule`). 40 → 0 compile errors.
5. **Delay-sign with `ecma.pub`** (`-delaysign+ -keyfile:…/ecma.pub`) — correct strong-name token
   (`b77a5c561934e089`), overriding AssemblyInfo's relative path. Produces the DLL.

Defines: the full `net_4_x` set from `net_4_x.make` + `class/corlib/Makefile` (`FEATURE_*`,
`MONO_FEATURE_*`, `INSIDE_CORLIB`, `GENERICS_WORK`, …), `BIT64` omitted (i686 target). `Consts.cs`
generated from `Consts.cs.in` with the pinned corlib version.

**No fundamental incompatibility** — Roslyn 13 builds Mono 6.13 corlib cleanly. The build inits
`external/{corefx,corert,referencesource}` automatically.

### Next: load it
1. Rebuild metadata (correct `MONO_CORLIB_VERSION` now in `config.h`) + relink the host.
2. Host: `mono_set_assemblies_path()` / `mono_assembly_setrootdir()` → the DVD dir; bundle
   `mscorlib.dll` in the ISO next to `default.xbe`.
3. Boot → `mono_jit_init` should return non-NULL (corlib loaded, version matches). Expect a second
   round of runtime icall gaps to surface (replace stubbed icalls that corlib init actually calls).
4. Load + interpret a trivial app assembly → managed output on serial.

## Milestone ladder (Phase 1b)

1. Build `mscorlib.dll` with Roslyn (iterate to a clean assembly).
2. Rebuild metadata (correct `MONO_CORLIB_VERSION`) + relink the host.
3. Bundle `mscorlib.dll` in the ISO; host sets the assembly root to the DVD; `mono_jit_init` returns
   non-NULL on real HW/xemu.
4. Load a trivial app assembly (C# `Main` writing a line) and execute it via the interpreter →
   **managed output on the xemu serial UART.**
5. Then: broaden the BCL surface, replace stubbed icalls as needed, and start on the actual
   example title.

## Corlib-load bring-up status (2026-09-27) — runtime init runs; corlib open is the last blocker

The host boots, `mono_jit_init` **runs to completion and returns cleanly** (no crash). Everything
below corlib now works on real-Xbox HLE:

- **Env layer:** `monoeg_g_getenv`/`g_hasenv` overridden to "always unset" (Xbox has no environment)
  — killed the spurious `DUMP_CROSS_OFFSETS` dump and the misread `MONO_PATH`/`MONO_GC_PARAMS`.
- **Win32 PAL fully activated:** `HAVE_CLASSIC_WINAPI_SUPPORT=1` (flips all 73 `w32subset.h` gates on
  so Mono calls the real APIs, not the "unsupported" stubs) + the Win32 constants
  (`FORMAT_MESSAGE_*`, `MWMO_*`, `SEM_*`, `FILE_MAP_*`, `COINIT_*`, …) + link shims
  (`CreateSemaphoreW`, `SetErrorMode`, `MsgWaitForMultipleObjectsEx`, dynamic-loading/COM/VEH stubs).
  The earlier `'CreateSemaphore … not supported'` crash is gone.
- **corlib built + bundled:** `mscorlib.dll` in the ISO at `D:\assemblies\`; `mono_set_assemblies_path`
  points there (and the `g_file_test` warning is non-fatal — assembly.c keeps the path).

**The remaining blocker — RXDK's WIDE (W) Win32 file APIs are broken; Mono (UNICODE) uses them.**
Verified via an on-device probe: `GetFileAttributesA("D:\assy\mscorlib.dll")` → **FOUND**, but the
**wide** `GetFileAttributesW` returns INVALID (and `\Device\CdRom0`/`\??\D:` forms are INVALID — the
drive-letter form via the *A* API is correct). Mono, being a UNICODE build, calls the W variants
everywhere (`g_file_test`→`GetFileAttributesW`, image open→`CreateFileW`+`CreateFileMappingW`).
Thunked so far (W→A, loose objects winning `--allow-multiple-definition`): `GetFileAttributesW`,
`CreateFileW`, `CreateFileMappingW`; also un-deferred `gfile-win32.c` (real `g_file_test`) and moved
`mono_stubs.o` after the archive group so real symbols win over stubs. corlib load still returns
NULL silently (Mono's own trace is off because env is disabled / the trace setters appear stubbed).

**Recommended finish — flip Mono to a NON-UNICODE (ANSI) build.** Rather than thunk every wide file
API (whack-a-mole: `MapViewOfFile*`, `FindFirstFileW`, `GetFullPathNameW`, …), drop `UNICODE`/
`_UNICODE` from `config.h` so Mono uses the **A** variants that RXDK actually implements. Handle the
one thing UNICODE was added for (the `WIN32_FIND_DATA` A/W typedef) by providing `WIN32_FIND_DATAW`
as an alias/type. This is the likely root fix. Secondary: get a working Mono log sink (route
`g_print`/`mono_trace` to `DbgPrint`) so corlib-load failures are visible without the environment.
