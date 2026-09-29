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
| `rxdk-dotnet-runtime.zip` | `assemblies/` holds `mscorlib.dll`, `System.dll`, `System.Core.dll`, `Rxdk.Input.dll`, `Rxdk.Kernel.dll`, and `Main.dll`. `lib/` holds the native runtime archives. |

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

Compile with `-target:exe` and name the output `Main.dll`. `Console.WriteLine` shows up on the
debug output.

Unzip `rxdk-dotnet-runtime.zip` and compile against the assemblies in `assemblies/`. Reference
this `mscorlib.dll` with `-nostdlib`. There is one `mscorlib.dll`, next to your program.

`App.cs`:

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

Point `csc.dll` at the SDK directory from `dotnet --list-sdks` (the path under your dotnet
install, for example `C:\Program Files\dotnet\sdk\10.0.400` on Windows). From the folder that
contains `assemblies/`:

```bash
dotnet exec "$CSC" -nostdlib -noconfig -target:exe -optimize+ -unsafe \
  -reference:assemblies/mscorlib.dll -reference:assemblies/System.dll \
  -out:assemblies/Main.dll App.cs
```

Add `-reference:assemblies/System.Core.dll` if the app uses `System.Linq`. Sockets, DNS, and
`System.Net.Sockets.Socket` live in `System.dll` (those calls are libxnet, not the kernel).
Add `-reference:assemblies/Rxdk.Input.dll` for `Rxdk.GamePad` and `Rxdk.Keyboard`, and
`-reference:assemblies/Rxdk.Kernel.dll` for kernel methods such as `Kernel.IoCreateSymbolicLink`
and `Kernel.AvSetDisplayMode`. The prebuilt `default.xbe` already contains that native code,
because `libxapi` and `libkernel` are in every title. Copy `Rxdk.Input.dll` and `Rxdk.Kernel.dll`
onto the disc with `Main.dll`.

## Copy it to the Xbox

`D:\` is the title drive: the DVD when you boot the ISO, or the title directory when you launch
from the hard disk. It is read-only from disc. `T:\` is the title's persistent partition and is
writable (`File.WriteAllText(@"T:\notes.txt", "...")`).

### xemu

Replace `assemblies/Main.dll` inside the disc tree and pack a new ISO with `xdvdfs`:

```text
RxdkMonoHost/
  default.xbe
  assemblies/
    mscorlib.dll
    System.dll
    System.Core.dll
    Rxdk.Input.dll
    Rxdk.Kernel.dll
    Main.dll
```

```bash
xdvdfs pack RxdkMonoHost RxdkMonoHost.iso
```

Boot from the xemu directory so its `xemu.toml` paths resolve. `-serial stdio` is the debug output:

```bash
xemu -dvd_path /path/to/RxdkMonoHost.iso -device lpc47m157 -serial stdio
```

The title reboots when it finishes, so the log repeats until you stop xemu.

### A devkit

`xbcp`, `xbset`, and `xbox-launch` are in the same tools zip. Xbox paths use the `xE:\` form.
Set the kit once, then copy the same tree the ISO contains and launch it. On a hard-disk launch,
`D:\` is that directory.

```bash
xbset 192.168.1.10
xbcp /y RxdkMonoHost.xbe xE:\devkit\RxdkMonoHost\default.xbe
xbcp /y /t assemblies xE:\devkit\RxdkMonoHost\assemblies
xbox-launch /dir xE:\devkit\RxdkMonoHost /title default.xbe
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

## What's next

The runtime runs your program. A game still needs the Xbox libraries and a small framework on top of them.

- **Library bindings.** Done: `Rxdk.Input`, `Rxdk.Kernel`, `Rxdk.Graphics`, `Rxdk.Audio`, `Rxdk.Music`, `Rxdk.Xact`, `Rxdk.Video`, `Rxdk.Uix`, `Rxdk.Online`, and `Rxdk.Voice`. Each library is its own assembly, and the host links that native library only when the assembly is on the disc. What remains is exercising the newer ones on a console and giving the raw calls friendlier managed types.
- **A cleaned MonoGame for original Xbox.** A Team-Resurgent fork on an `xbox` branch, with the other platforms and their desktop graphics stacks removed. It calls these bindings. Content is still built on the PC. The title reads the built files from `D:\`.
- **A normal project.** `dotnet build` of a C# project that references `mscorlib.dll`, produces `Main.dll`, and packs or copies the title. The hand-written `csc` line above is the stand-in until that exists.
- **Visual Studio and VS Code.** New extensions for this runtime: a project template, build, deploy to a kit or xemu, and debug.
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
bash scripts/build-input.sh
bash scripts/build-kernel.sh
bash scripts/build-graphics.sh
bash scripts/build-sdklibs.sh
bash scripts/build-testasm.sh
bash scripts/build-minitests.sh
bash scripts/build-host.sh
```

The ISO is `build-out/obj/host/RxdkMonoHost.iso`.
