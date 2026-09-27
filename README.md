# RXDK-DotNet

Exploratory effort to bring a **managed .NET runtime to the original Xbox**, built on the
[RXDK-SDK](https://github.com/Team-Resurgent/RXDK-SDK) (the prebuilt `.lib`s + headers from the
MSVC-free RXDK-Libs clang/lld runtime). The runtime's core PAL favors `xboxkrnl` **kernel
imports** (`libkernel`/`libc`/`libcpp`) to stay lean, while off-critical-path needs — input
(XInput/keyboard), TLS, audio, graphics — come from the SDK's `libxapi`/`libd3d8`/`libdsound`
rather than being rebuilt. CI/release is appropriated from RXDK-Tools, with a hard **isa-scan
PIII gate** on all native output.

**Status: planning.** No runtime code yet. Start with the plan:

- [`docs/port-plan.md`](docs/port-plan.md) — the full port plan: what we inherit from
  RXDK-Libs, the hardware constraints, the pivotal **SSE2** decision, a Mono-vs-NativeAOT
  comparison, the PAL architecture, phased milestones, and the risk register.

The runtime choice (Mono vs NativeAOT) is intentionally **undecided** and gated behind a
Phase-0 spike (the OG Xbox's Pentium III is SSE1-only; modern .NET x86 codegen assumes SSE2).
