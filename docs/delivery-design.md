# Delivery design — how a user compiles an Xbox .NET project

Goal: **a C# developer, on Windows / Linux / macOS, builds an Xbox title from a normal
C# project with one command (or one click) and never touches clang / lld / imagebld / mono
internals.** They write C#, run a build, and get a bootable `.xbe` / `.iso`.

This is a forward-looking design; it's recorded now because it shapes how the runtime and
tooling are structured (see also `port-plan.md`).

## The pipeline we're hiding

```
  user's C# (.csproj, netstandard2.0)
        │  Roslyn / dotnet build          (cross-platform, stock .NET SDK)
        ▼
  IL assemblies (.dll)
        │  RXDK-DotNet build step
        ▼
  ┌─ AOT path:   mono cross-AOT (IL → i686 .o) ─┐
  │                                              ├─ link w/ Mono runtime + PAL + RXDK-SDK
  └─ interp path: bundle IL into the image ──────┘        │  clang/lld (RXDK LLVM toolchain)
                                                          ▼
                                                    PE  →  imagebld  →  .xbe
                                                          →  xdvdfs  →  .iso
```

The bottom half (clang/lld/imagebld/xdvdfs) is **already solved and cross-platform** — the
`rxdk` engine drives it today (proven: `tests/hello-c` builds a real `.xbe`+`.iso`). Our new
work is the middle: turn IL + the Mono-for-Xbox runtime into something that engine can link.

## Delivery vehicles (layered, all cross-platform)

1. **MSBuild SDK — the primary "easy" path.** An SDK-style project:
   ```xml
   <Project Sdk="Rxdk.DotNet.Sdk">
     <PropertyGroup><TargetFramework>netstandard2.0</TargetFramework></PropertyGroup>
   </Project>
   ```
   `dotnet build` produces an `.xbe`/`.iso`. Familiar to every C# dev, runs on any OS the
   .NET SDK runs on, no bespoke CLI to learn. Under the hood the SDK's targets invoke the
   `rxdk` engine's managed-build path.
2. **`rxdk` engine — the workhorse.** It already builds native titles cross-platform and reads
   `rxdk.project.json`. Add a **"managed title" build type**: consume the `.csproj`'s IL output,
   run the AOT/bundle step, then reuse the existing native link→XBE→ISO pipeline. `rxdk build`
   / `rxdk launch-xemu` / `rxdk deploy` then work for .NET projects exactly as for C titles.
3. **IDE extensions (VS Code + VS20xx) — the one-click layer.** RXDK already ships
   **RXDK-VSCode** and **RXDK-VS20XX**; add .NET project **templates** + build/run/deploy/xemu
   actions there. This is the eventual "File → New → Xbox .NET Project" experience. (Your idea —
   it's the right long-term front end.)
4. **`dotnet new` templates + NuGet.** Ship the SDK and templates via NuGet so
   `dotnet new xbox-console` scaffolds a ready project. Cross-platform by construction.

## What we prebuild and ship (so users don't)

- **Mono-for-Xbox runtime + PAL** — a prebuilt `libmono` package (the way RXDK-SDK ships
  `.lib`s), so users never build mono. Pinned, versioned, downloaded on first use.
  **Build/release infra (mirrors the LLVM model):** `Team-Resurgent/mono` has an orphan,
  CI-only **`teamresurgent`** branch (no mono source) whose workflow cross-builds the **`xbox`**
  branch and publishes `libmono` as a rolling GitHub release; RXDK-DotNet consumes that release.
  Exactly how `Team-Resurgent/llvm-project`'s `teamresurgent` branch builds its `xbox` branch and
  releases the clang toolchain. The workflow's build steps are filled in once the Phase-1 recipe
  (`phase1-mono.md`) is proven locally, so CI encodes a known-good build rather than a guess.
- **RXDK LLVM toolchain** — already released as per-host zips (win/linux/macOS × x64/arm64) and
  auto-installed via `rxdk install-llvm`. Reuse verbatim.
- **RXDK-SDK** (`libxapi`/`libd3d8`/… + headers) — already staged via `rxdk install-sdk`.

## AOT vs interpreter has a *delivery* dimension (not just perf)

This is worth weighing alongside the perf/memory tradeoff in `port-plan.md` §4:

- **Interpreter delivery is simpler & more portable.** The host side is just `dotnet build`
  (C#→IL, already cross-platform) + bundle IL into the image. **No per-host cross-compiler.**
  The Mono runtime is a *target-only* artifact, built once. So "works on any dev OS" falls out
  almost for free.
- **AOT delivery needs a per-host cross-compiler.** The mono cross-AOT compiler (IL→i686) must
  run on the developer's machine and be built for each host OS/arch — mirroring the LLVM
  per-host release model. More build infra, more to ship, more to keep in sync.

So: **interpreter is the easier thing to deliver cross-platform**, AOT is the faster thing to
run. A likely end state is **AOT with an interpreter fallback**, but the interpreter is the
lower-friction first delivery — which also lines up with the net8 direction (net8 is
interpreter-only on this CPU anyway; `port-plan.md` §4).

## Guiding principle

The user's mental model is stock .NET: *write C#, `dotnet build`, get an artifact.* Every layer
above exists to preserve that illusion on a 2001 console. Anything that leaks clang flags, mono
build steps, or XBE plumbing to the user is a bug in the delivery design.
