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

**In progress:** enabling the JIT (x87 codegen) in the standard `--interpreter` configuration — the
interpreter needs the JIT to compile its native entry trampolines (`interp_in` wrappers), so general
managed code (any class with a static constructor) requires it. See [`docs/`](docs/).

Build/run: `scripts/build-*.sh` compile the runtime layers, the corlib, and the test assembly, then
package a bootable XBE/ISO; boot with `xemu -dvd_path <iso> -device lpc47m157 -serial stdio`.

- [`docs/port-plan.md`](docs/port-plan.md) — the full port plan, hardware constraints, the **SSE2**
  decision, PAL architecture, phased milestones, and the risk register.
