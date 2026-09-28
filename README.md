# RXDK-DotNet

Exploratory effort to bring a **managed .NET runtime to the original Xbox**, built on the
[RXDK-SDK](https://github.com/Team-Resurgent/RXDK-SDK) (the prebuilt `.lib`s + headers from the
MSVC-free RXDK-Libs clang/lld runtime). The runtime's core PAL favors `xboxkrnl` **kernel
imports** (`libkernel`/`libc`/`libcpp`) to stay lean, while off-critical-path needs — input
(XInput/keyboard), TLS, audio, graphics — come from the SDK's `libxapi`/`libd3d8`/`libdsound`
rather than being rebuilt. CI/release is appropriated from RXDK-Tools, with a hard **isa-scan
PIII gate** on all native output.

**Status: the Mono runtime boots and runs managed code on the original Xbox (xemu).**

Runtime choice resolved to **classic Mono 6.13** (x87 FP backend, interpreter-first), built MSVC-free
with the RXDK clang/lld toolchain against a hand-written PAL over `xboxkrnl` + `libxapi`. What works
today, verified on xemu:

- **Full runtime init** — sgen GC, thread attach, metadata/loader/reflection/icalls; the app domain
  is created and `mono_jit_init_version` returns a live domain.
- **corlib loads** — a classic-Mono `mscorlib.dll` built with Roslyn, loaded from the DVD via a
  read-based file-mapping shim (the Xbox has no `CreateFileMapping`).
- **Managed IL executes on the interpreter** — e.g. `Fib(20) = 6765`, arithmetic, loops, and
  value-type marshaling, invoked through `mono_runtime_invoke`.
- **Managed `Console` output → debug serial** — `Console.Write`/`WriteLine` reach the UART through
  real console handles in the PAL.

- **JIT enabled (x87 codegen) in the `--interpreter` configuration** — the interpreter needs the JIT
  to compile wrappers/trampolines; it compiles and links, and generates native code.
- **A managed self-test passes 43/43** on-device (`tests/managed/Test.cs`): **delegates**
  (`Func<...>` via the native→interp trampoline), **`System.Console.WriteLine`** (to the debug UART),
  integer/long/ulong and **x87 float+double** arithmetic, **float/double `ToString`**, bitops/shifts,
  arrays + bounds exceptions, foreach, jagged arrays, strings (`Substring`/`ToUpper`/`Split`/`Trim`/
  `StartsWith`), `int.Parse`/`int.ToString`, **generics** (`List<T>`), structs, boxing, static/instance
  fields, **virtual + interface dispatch**, enums, switch, recursion, ref/out, params, full **exception
  handling** (try/catch/finally, rethrow, null-ref, div-by-zero), **`DateTime.Now`**, and **file I/O
  reads** (`File.Exists`, `FileStream`, `Directory.Exists`).

- **Mono's own JIT regression suite runs on-device — 711/711, 0 failures.** The upstream tests from
  [`mono/mini/*.cs`](vendor/mono/mono/mini) are built with Roslyn against our `mscorlib` and driven
  by the host through the **unmodified** upstream `TestDriver.cs` (reflection + `MethodInfo.Invoke`,
  run with `--time`): `basic` 134, `basic-long` 97, `basic-float` (x87) 58, `basic-math` 27,
  `arrays` 36, `objects` 105, `exceptions` 85, `builtin-types` 84, `devirtualization` 6,
  `generics` 79. `generics` needs a minimal **System.Core** (LINQ-to-objects,
  [`scripts/build-syscore.sh`](scripts/build-syscore.sh)). See
  [`scripts/build-minitests.sh`](scripts/build-minitests.sh).

- **`DateTime.Now` / `TimeZoneInfo` work** — the local offset comes from the Xbox EEPROM time zone
  (`kernel32!GetTimeZoneInformation` in libxapi), resolved through a Mono dynamic-loader fallback that
  maps `DllImport` targets to linked-in functions, since the Xbox has no user-mode loader.

- **File I/O reads** — `File.Exists`, `FileStream` reads, and `Directory.Exists` work off the DVD.
  The corlib file stack is mixed (corefx `File`/`Directory` P/Invoke `kernel32` directly; `FileStream`
  is Mono's `MonoIO`), both served over the RXDK ANSI Win32 APIs via `WIN32_FIND_DATAW`/W→A thunks
  ([`pal/src/win32_file_shims.c`](pal/src/win32_file_shims.c)) and the P/Invoke fallback.
  *Follow-ups:* `Directory.GetFiles` enumeration (corefx uses ntdll `NtQueryDirectoryFile`, not yet
  wired) and writes (the title drive is read-only when booted from disc).

## Build & run

Each `scripts/build-*.sh` compiles one layer (they don't auto-chain). Typical order after a clean
checkout, and what to re-run after editing a given area:

| Script | Builds | Re-run when you touch | Approx |
|---|---|---|---|
| [`scripts/build-eglib.sh`](scripts/build-eglib.sh)     | eglib (`monoeg_g_*`)            | eglib sources                          | ~20s |
| [`scripts/build-utils.sh`](scripts/build-utils.sh)     | `libmonoutils` (incl. `mono-dl`)| `vendor/mono/mono/utils/*`, its excludes| ~90s |
| [`scripts/build-metadata.sh`](scripts/build-metadata.sh)| `libmonoruntime` (sgen + metadata, incl. `w32file-win32`, `icall-windows`) | `vendor/mono/mono/metadata/*`, `sgen/*`, its excludes | ~2–3 min |
| [`scripts/build-mini.sh`](scripts/build-mini.sh)       | `libmini` (JIT + interp)         | `vendor/mono/mono/mini/*` (e.g. `interp/transform.c`) | ~90s |
| [`scripts/build-corlib.sh`](scripts/build-corlib.sh)   | `mscorlib.dll` (Roslyn)          | corlib sources / defines               | fast |
| [`scripts/build-syscore.sh`](scripts/build-syscore.sh) | minimal `System.Core.dll` (LINQ) | corefx System.Linq / `build/managed/syscore-shims.cs` | fast |
| [`scripts/build-testasm.sh`](scripts/build-testasm.sh) | `Test.dll` (self-test)           | `tests/managed/Test.cs`                | fast |
| [`scripts/build-minitests.sh`](scripts/build-minitests.sh) | `mini-*.dll` (official suite) | curated test list                      | fast |
| [`scripts/build-host.sh`](scripts/build-host.sh)       | links the XBE, packages the ISO  | `pal/src/*`, `tests/mono-host/host_main.c`, or any lib above | ~15s |

### Running on xemu

Testing is done on [xemu](https://xemu.app) (a **devkit build** — this project's checkout uses
`D:\Git\xemu-devkit`). The dev environment there is a Cerbios BIOS + an insignia HDD image, all wired
up in `xemu.toml`, so you don't pass a BIOS/HDD on the command line — only the ISO.

**Run from the xemu directory** (the `roms\` paths in `xemu.toml` are relative), and route the guest's
debug UART to stdout with `-serial stdio` — that's where all the host's `OutputDebugStringA` /
`Console` output and the test `PASS/FAIL` lines appear:

```bash
cd /d/Git/xemu-devkit
./xemu.exe -dvd_path /d/Git/RXDK-DotNet/build-out/obj/host/RxdkMonoHost.iso -device lpc47m157 -serial stdio
```

The XBE runs its tests and exits via `HalReturnToFirmware(QuickReboot)`, so **the box reboots and
reloads the DVD in a loop** — expect the serial output to repeat. Capture one run and stop:

```bash
cd /d/Git/xemu-devkit
timeout 45 ./xemu.exe -dvd_path /d/Git/RXDK-DotNet/build-out/obj/host/RxdkMonoHost.iso \
  -device lpc47m157 -serial stdio > /tmp/serial.log 2>&1
grep -aE 'PASS|FAIL|EXC|SUMMARY' /tmp/serial.log | grep -v 'mono:debug'
```

For a **hard fault with no serial output** (the box resets before printing), add `-d int -D int.log`
to log guest CPU exceptions with the faulting IP/SP/CR2 — `v=0e` page fault, `v=08` double fault,
`v=07` x87-FPU. `IP=00000001` means a call through a bad/sentinel function pointer (e.g. a
calling-convention mismatch); a kernel-space caller `EIP=8001xxxx` points at a libxapi function.

**Gotchas that will bite a fresh session** (see the `memory/` notes below):
- **Kill `clang`/`llvm-lib` before `build-host`** and verify a probe string landed in the exe
  (`grep -c <probe> build-out/obj/host/mono-host.exe`) — stale-link races produce confusing results.
- **Stub-shadowing landmine** ([`pal/src/mono_stubs.c`](pal/src/mono_stubs.c)): no-op `int NAME(void){return 0;}`
  stubs are linked after the archive group with `--allow-multiple-definition`. When a TU starts
  compiling and provides the *real* symbol, its matching stub must be commented out or it silently
  wins (returns 0). This has caused ~6 deep bugs already.
- **P/Invoke fallback** ([`pal/src/win32_supplement.c`](pal/src/win32_supplement.c) `rxdk_dl_symbol`):
  the Xbox has no user-mode loader, so `DllImport("kernel32"/"advapi32")` targets resolve through a
  `mono_dl` fallback to linked-in functions. `SetLastError=true` DllImports are called **cdecl**
  (caller-cleanup) on this build — their thunks must be cdecl or the stack corrupts (jump to `IP=1`).
- Commit messages carry **no** `Co-Authored-By`. Mono changes go on the `xbox` branch of the
  `vendor/mono` submodule (default CI branch is `teamresurgent`).

## Next steps (roadmap for a follow-up session)

Ordered roughly by value / tractability. Each names the concrete files to touch.

1. **File writes** — reads work; writes need a *writable* volume (the title drive `D:\` is read-only
   when booted from DVD). Use **`T:\`** — the title's per-title persistent HDD partition (writable on
   the devkit/xemu HDD image). Test `File.WriteAllText`/`FileStream` write + read-back to `T:\`. The
   write path is already wired (`mono_w32file_write` in
   [`pal/src/win32_supplement.c`](pal/src/win32_supplement.c) → `WriteFile`; corefx `WriteFile` via the
   fallback), and `CreateFileW` (open/create) is thunked — so this is mostly picking `T:\` and testing.
2. **`Directory.GetFiles` / enumeration** — corefx `FileSystemEnumerator` uses **ntdll
   `NtQueryDirectoryFile`** (currently unwired → OOM). Wire `NtQueryDirectoryFile`/`NtCreateFile`
   (xboxkrnl exports them) in [`pal/src/win32_file_shims.c`](pal/src/win32_file_shims.c) + register in
   the fallback, or redirect corefx enumeration to a `FindFirstFile`-style path. `FileInfo.Length`
   also needs `GetFileAttributesExW` to fill the size (left 0 today — opening for `GetFileSize` faulted;
   revisit).
3. **Crypto RNG** — `mono_rand_*` are stubbed (return 0 → zero entropy), breaking `Guid.NewGuid`,
   `Random`, `RNGCryptoServiceProvider`. Un-stub in [`pal/src/mono_stubs.c`](pal/src/mono_stubs.c) and
   implement over an Xbox entropy source (`XeCryptRandom`/RDTSC-seeded PRNG) in the PAL.
4. **Culture data** — `ves_icall_System_Globalization_Culture*/Calendar*/RegionInfo_fill_*` are
   stubbed; invariant formatting works (ordinal `CompareInfo` + `_ecvt_s`), but culture-aware
   format/parse and `CultureInfo.GetCultures` don't. Needs locale tables.
5. **zlib** — `inflate`/`inflateInit2_` stubbed → `DeflateStream`/`GZipStream` don't work. Build the
   bundled zlib or map to a real one.
6. **More mini suites** — add to [`scripts/build-minitests.sh`](scripts/build-minitests.sh): the
   remaining `vendor/mono/mono/mini/*.cs` (e.g. `gshared`, `ratests`) as corlib support allows.
7. **Networking** — `Socket`/`Dns` icalls + `w32socket*` excluded. Large but self-contained; the Xbox
   NIC works (xemu shows DHCP).

A **Claude Code session on this machine** also carries persistent notes (its `memory/` store:
`file-io.md`, `pinvoke-fallback.md`, `mono-stubs-shadowing.md`, `phase1b-corlib-loads.md`) with the
deep on-device debugging details behind each of the above — a fresh session loads them automatically.

- [`docs/port-plan.md`](docs/port-plan.md) — the full port plan, hardware constraints, the **SSE2**
  decision, PAL architecture, phased milestones, and the risk register.
