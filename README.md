# RXDK-DotNet

<p align="center"><b>Managed .NET on the original Xbox — compile on your PC, boot an ISO in xemu, or copy the title to a devkit</b></p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/RXDK-DotNet/actions/workflows/build.yml"><img src="https://github.com/Team-Resurgent/RXDK-DotNet/actions/workflows/build.yml/badge.svg" alt="Build"></a>
  <a href="https://github.com/Team-Resurgent/RXDK-DotNet/releases/latest"><img src="https://img.shields.io/github/v/release/Team-Resurgent/RXDK-DotNet?label=Release" alt="Release"></a>
  <a href="https://discord.gg/VcdSfajQGK"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/J3J7L5UMN"><img src="https://img.shields.io/badge/ko--fi-Support-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white" alt="ko-fi"></a>
  <a href="https://www.patreon.com/teamresurgent"><img src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

Classic Mono 6.13 runs your IL on the interpreter.

A push to `main` publishes a moving
[latest](https://github.com/Team-Resurgent/RXDK-DotNet/releases/latest) release:

| File | What it is |
|---|---|
| `RxdkMonoHost.iso` | Bootable disc. This is the file xemu runs. |
| `RxdkMonoHost.xbe` | The title inside that ISO. |
| `rxdk-dotnet-runtime.zip` | `assemblies/` holds `mscorlib.dll`, `System.dll`, `System.Core.dll`, `System.Numerics.Vectors.dll`, `System.Runtime.Serialization.dll`, `MonoGame.Framework.dll`, the `Rxdk.*` libraries, and `Main.dll`. `msbuild/` holds the two files a title project imports. `lib/` holds the native runtime archives. |

The Xbox image is built once. The PC-side tools you use to pack a disc or copy files onto a kit
are the [RXDK Tools](https://github.com/Team-Resurgent/RXDK-Tools/releases/latest) packages, one
per machine:

| Your PC | Download |
|---|---|
| Windows x64 | `rxdk-managed-win-x64.zip` |
| Windows arm64 | `rxdk-managed-win-arm64.zip` |
| Linux x64 | `rxdk-managed-linux-x64.zip` |
| Linux arm64 | `rxdk-managed-linux-arm64.zip` |
| macOS Intel | `rxdk-managed-osx-x64.zip` |
| macOS Apple Silicon | `rxdk-managed-osx-arm64.zip` |

Each zip has `tools/` (`xbcp`, `xbset`, `imagebld`, `xbox-launch`, …) and a .NET 8
runtime installer (`install-dotnet-runtime.cmd` or `install-dotnet-runtime.sh`). `xdvdfs` is a
separate download from
[XDVDFS-TR](https://github.com/Team-Resurgent/XDVDFS-TR/releases/latest)
(`xdvdfs-windows-x64`, `xdvdfs-windows-arm64`, `xdvdfs-linux-x64`, `xdvdfs-linux-arm64`,
`xdvdfs-macos-x64`, `xdvdfs-macos-arm64`). Compiling C# uses the .NET SDK you already have.

## Compile an app

`default.xbe` is the process. After Mono loads `mscorlib` from `D:\assemblies`, that host opens
`D:\assemblies\Main.dll` and runs its entry point, `static int Main(string[] args)`. The class
name does not matter. `Main` gets an empty `args` array. Its return value is printed on the
debug output as `Main returned N`. A thrown exception stops there and prints `Main threw`.

`Console.WriteLine` shows up on the debug output.

A title is an ordinary C# project. `samples/HelloXbox` is the whole of it: a `Program.cs` and a
`.csproj` that imports two files from `msbuild/`.

`Program.cs`:

```csharp
using System;

class Program
{
    static int Main(string[] args)
    {
        Console.WriteLine("hello from the Xbox");
        return 0;
    }
}
```

`HelloXbox.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <Import Project="../../msbuild/Rxdk.Title.props" />
  <Import Project="../../msbuild/Rxdk.Title.targets" />
</Project>
```

`Rxdk.Title.props` sets the compile shape (`NoStdLib`, no implicit framework references, output
named `Main.dll`) and `Rxdk.Title.targets` references every assembly in the runtime and adds two
targets:

```bash
dotnet build                 # Main.dll, compiled against the runtime's mscorlib
dotnet build -t:RxdkStage    # bin/Debug/disc/ -- default.xbe plus assemblies/
dotnet build -t:RxdkPack     # bin/Debug/<project>.iso, bootable
```

`RxdkPack` downloads the `xdvdfs` build for the machine it runs on into `~/.rxdk/tools` and caches
it, so the same three commands work on Windows, macOS, and Linux on x64 and arm64 with nothing
installed but the .NET SDK.

Outside a checkout of this repo, unzip `rxdk-dotnet-runtime.zip`, copy the two `msbuild/` files
next to the project, and set `RxdkRuntimeDir` to the folder holding `assemblies/` along with
`RxdkXbe` to `RxdkMonoHost.xbe`. Every assembly is referenced whether or not the title uses it;
an unused reference does not reach the output, but `RxdkStage` does copy them all to the disc.
Add files to the disc root with `<RxdkContent Include="..." />`.

Sockets, DNS, and `System.Net.Sockets.Socket` live in `System.dll` (those calls are libxnet, not
the kernel). `System.Linq` is in `System.Core.dll`. `Rxdk.Input.dll` has `Rxdk.GamePad` and
`Rxdk.Keyboard`; `Rxdk.Kernel.dll` has kernel methods such as `Kernel.IoCreateSymbolicLink` and
`Kernel.AvSetDisplayMode`. The prebuilt `default.xbe` already contains that native code, because
`libxapi` and `libkernel` are in every title.

To compile without MSBuild, reference the runtime's `mscorlib.dll` with `-nostdlib`. Point `$CSC`
at `csc.dll` under the SDK directory from `dotnet --list-sdks`:

```bash
dotnet exec "$CSC" -nostdlib -noconfig -target:exe -optimize+ -unsafe \
  -reference:assemblies/mscorlib.dll -reference:assemblies/System.dll \
  -out:assemblies/Main.dll Program.cs
```

## Copy it to the Xbox

`D:\` is the title drive: the DVD when you boot the ISO, or the title directory when you launch
from the hard disk. It is read-only from disc. `T:\` is the title's persistent partition and is
writable (`File.WriteAllText(@"T:\notes.txt", "...")`).

Either target produces the same tree, `RxdkPack` just wraps it in an ISO:

```text
disc/
  default.xbe
  assemblies/
    mscorlib.dll
    System.dll
    System.Core.dll
    Rxdk.Input.dll
    Rxdk.Kernel.dll
    Main.dll
```

### xemu

Boot from the xemu directory so its `xemu.toml` paths resolve. `-serial stdio` is the debug output:

```bash
xemu -dvd_path bin/Debug/HelloXbox.iso -device lpc47m157 -serial stdio
```

The title reboots when it finishes, so the log repeats until you stop xemu.

### A devkit

`xbcp`, `xbset`, and `xbox-launch` are in the same tools zip. Xbox paths use the `xE:\` form.
Set the kit once, then copy the staged tree and launch it. On a hard-disk launch, `D:\` is that
directory.

```bash
xbset 192.168.1.10
xbcp /y /t bin/Debug/disc xE:\devkit\HelloXbox
xbox-launch /dir xE:\devkit\HelloXbox /title default.xbe
```

`/x <ip-or-name>` on any of those commands overrides the default kit. Debug text goes to
`xbwatson`.

## What an app can use

Verified on xemu: arithmetic, strings, arrays, generics (`List<T>`), delegates, virtual calls,
exceptions, `DateTime.Now`, `Guid.NewGuid`, `CultureInfo` (`new CultureInfo("en-US")`;
`CurrentCulture` stays invariant), file reads on `D:\`, file writes on `T:\`,
`Directory.GetFiles`, `DeflateStream` / `GZipStream` when those types are in your assembly,
embedded manifest resources (`Assembly.GetManifestResourceStream`),
UDP and TCP through `System.Net.Sockets.Socket`, DNS (`Dns.GetHostName()` is `xbox`),
`Rxdk.GamePad` / `Rxdk.Keyboard` / `Rxdk.Mouse` / `Rxdk.IrRemote` (`Open`, `GetState`, `Dispose`),
and `Rxdk.Kernel`. xemu port 1 is `GamePad.Open(0)`. Input is started with `XInitDevices`, then
the title waits until device enumeration is idle before reading a device. An empty port reports
`IsConnected == false`. `GamePad.Kind` tells a wheel, light gun, arcade stick, dance pad, and the
other pad-shaped controllers apart from a standard pad. Light-gun aim is the thumb-stick axes,
and `SetLightgunCalibration` stores the offsets. Mouse `X`/`Y`/`Wheel` are relative motion.

`Rxdk.Graphics` is one static device. `GraphicsDevice.Open` picks the first supported entry in
`Display.Defaults` (including 1920×1080; delete a line to drop that mode). The last line is the
fallback. On xemu that was `720x480 interlaced 4:3 60`. Model, view, and projection, the viewport,
and render state can be set and read back. `Clear` takes color, depth, and stencil.
`VertexBuffer`, `IndexBuffer`, and `Texture` are created separately (`Texture.FromFile` /
`FromMemory` load an image). `CubeTexture` loads the same way. `VolumeTexture.Create` allocates
a volume; the Xbox D3DX library has no volume-from-file loader. `XGraphics` covers swizzle,
compression, resource headers, indexed-draw push buffers, shader compile and splice, and saving
a texture to `.bmp` or `.xpr`. `MathHelper.ToRadian` / `ToDegree` and the short vector math are C#;
the rest of the XGraphics math calls `libxgraphics`. A title that references `Rxdk.Graphics.dll`
relinks the XBE with `libd3d8` and `libxgraphics`. Swapping `Main.dll` onto an ISO built without
that assembly cannot add Direct3D.

Shaders go onto the device from the same assembly. `XGraphics.CompileShader` takes HLSL and
`XGraphics.AssembleShader` takes shader assembly; both hand back microcode as a `byte[]`.
`GraphicsDevice.CreateVertexShader` turns a declaration and that microcode into a handle, which
`LoadVertexShader`, `SelectVertexShader`, and the `VertexShader` property put on the device, and
`CreatePixelShader` / `PixelShader` do the same for a pixel shader. `LoadVertexShaderProgram` and
`SetPixelShaderProgram` skip the handle and push a program straight through. Constants are
`SetVertexShaderConstant` / `SetPixelShaderConstant` and their getters, taking a `float[]` whose
length is a multiple of four. `DeleteVertexShader` and `DeletePixelShader` release a handle.
`SetVertexFormat` is unrelated: it selects a fixed-function vertex format, not a shader.

Seven more Xbox libraries are wrapped the same way, one assembly each: `Rxdk.Audio` (`libdsound`),
`Rxdk.Music` (`libdmusic`), `Rxdk.Xact` (`libxact`), `Rxdk.Video` (`libxmv`), `Rxdk.Uix` (`libuix`),
`Rxdk.Online` (`libxonline`), and `Rxdk.Voice` (`libxvoice` and the `XHVEngine` surface). Method
names match the native ones, strings are ordinary C# strings, pointers are `IntPtr`, and `BOOL` is
`bool`. `Rxdk.Music`, `Rxdk.Xact`, and `Rxdk.Video` also pull `libdsound`; `Rxdk.Uix` pulls
`libxonline`; `Rxdk.Online` pulls `libxneto`, the online build of `libxnet`. These assemblies are
generated by `scripts/gen-sdklibs.py`, which only emits a function when the header signature and
the library's own export agree on stack size, so nothing is guessed. They compile, link, and load
without disturbing a boot, but unlike the rest of this section no call into them has been run on a
console yet.

`libxbdm` is deliberately not wrapped. It holds no code, only imports bound to `xbdm.dll` by
ordinal, so a title that links it fails to load with `STATUS_ORDINAL_NOT_FOUND`. The build refuses
it rather than producing an XBE that cannot start.

`Rxdk.Kernel` is the xboxkrnl surface. Method names match the kernel. String arguments are
ordinary C# strings (`Kernel.IoCreateSymbolicLink(@"\??\RxdkLink", @"\Device\CdRom0")`), bools
are bools, and a returned handle is an `IntPtr` closed with `Kernel.NtClose`. File, directory,
thread, and socket code stays on those managed APIs. `Kernel.AvSetDisplayMode` reprograms the
display. `Kernel.HalReturnToFirmware` does not return (`Kernel.HalQuickRebootRoutine` reboots).

`127.0.0.1` is not a bindable address. Bind `0.0.0.0` and send to the title's own IPv4. The
serial log prints that address as it comes up (`RXDK-DotNet: net 192.168.1.96 dhcp 0x68`).
Binding `127.0.0.1` throws `SocketException` 10049.

## MonoGame

`MonoGame.Framework.dll` is in the runtime zip. It is MonoGame 3.8.5 from the Team-Resurgent fork
(`vendor/monogame`, `xbox` branch) with the other platforms removed and an Xbox backend over
`Rxdk.Graphics` and `Rxdk.Input`. A game's own code does not change.

`vendor/monogame-samples/Platformer2D/Platformer2D.Xbox` is the example: the upstream
Platformer2D sources, unmodified, plus a `.csproj` that imports the same two title files and
names the content project:

```xml
<ItemGroup>
  <Compile Include="../Platformer2D.Core/**/*.cs" />
  <RxdkMgcb Include="../Platformer2D.Core/Content/Platformer2D.mgcb" />
</ItemGroup>
```

`dotnet build -t:RxdkPack` builds the content with MGCB on the PC, installing `dotnet-mgcb` into
`~/.rxdk/tools` the first time, and stages the output under `D:\Content`. A project with a
`Content/*.mgcb` beside it does not need the `RxdkMgcb` line. On xemu the platformer draws its
level (the background layers, tiles, gems, player, and exit), the HUD font, and the timer counts
down to the lose screen. Its sound effects and music play.

What the backend expects of content and code:

- Textures are `Color` or DXT. A `Color` texture of any size works; one whose sides are not
  powers of two is padded on the console, so it costs the memory of the padded size and cannot be
  mipmapped. A DXT texture must be a power of two on both sides.
- Effects are `SpriteEffect` and `BasicEffect`. The NV2A has no HLSL compiler on the console and
  MGFX cannot target it, so a custom `.fx` does not load.
- Index buffers are 16-bit. Vertex and index buffers are write-only.
- `Texture2D.FromStream` needs an image decoder the console does not have. Load images through
  the content pipeline.
- `SoundEffect` plays through DirectSound (`Rxdk.Audio`), with volume, pitch, pan, and looping.
  It takes PCM and float WAVs; ADPCM sounds load but stay silent.
- `Song` streams the `.wma` beside its `.xnb` through the SDK's WMA decoder, on the game thread
  once a frame. `DynamicSoundEffectInstance` is still silent.

## What's next

The runtime runs your program. A game still needs the Xbox libraries and a small framework on top of them.

- **Library bindings.** Done: `Rxdk.Input`, `Rxdk.Kernel`, `Rxdk.Graphics`, `Rxdk.Audio`, `Rxdk.Music`, `Rxdk.Xact`, `Rxdk.Video`, `Rxdk.Uix`, `Rxdk.Online`, and `Rxdk.Voice`. Each library is its own assembly, and the host links that native library only when the assembly is on the disc. What remains is exercising the newer ones on a console and giving the raw calls friendlier managed types.
- **More of MonoGame.** Platformer2D runs (see [MonoGame](#monogame)). Left: custom effects, 3D samples beyond `BasicEffect`, and input checked on a console.
- **Visual Studio and VS Code.** New extensions for this runtime: a project template, build, deploy to a kit or xemu, and debug. `dotnet build` already builds and packs a title, so what is left is the editor surface: a template to create the project from, one-click deploy, and a debugger.
- **Saves.** A small API over `T:\`, the title's writable partition, so a game can store progress without inventing its own file layout.

## Build the runtime yourself

The shipped ISO is enough to compile an app and copy it over. Rebuilding the native runtime is
a Windows + Git Bash checkout of this repo: the scripts call `bash scripts/build-*.sh` and expect
the RXDK clang, SDK, and tools under `C:\ProgramData\RXDK`. The clang package that matches the PC
is `xbox-windows-x64.zip` or `xbox-windows-arm64.zip` from the
[llvm-project latest release](https://github.com/Team-Resurgent/llvm-project/releases/latest)
(the other four are `xbox-linux-x64`, `xbox-linux-arm64`, `xbox-macos-x64`, `xbox-macos-arm64`).
The SDK is [RXDK-SDK](https://github.com/Team-Resurgent/RXDK-SDK).

```bash
bash scripts/build-eglib.sh
bash scripts/build-utils.sh
bash scripts/build-metadata.sh
bash scripts/build-mini.sh
bash scripts/build-corlib.sh
bash scripts/build-syscore.sh
bash scripts/build-bcl.sh
bash scripts/build-input.sh
bash scripts/build-kernel.sh
bash scripts/build-graphics.sh
bash scripts/build-sdklibs.sh
bash scripts/build-monogame.sh
bash scripts/build-testasm.sh
bash scripts/build-minitests.sh
bash scripts/build-host.sh
```

The ISO is `build-out/obj/host/RxdkMonoHost.iso`.
