# RXDK-DotNet

Managed .NET on the original Xbox. Classic Mono 6.13 runs your IL on the interpreter. You compile
on your PC, then boot an ISO in xemu or copy the title onto a devkit.

A push to `main` publishes a moving
[latest](https://github.com/Team-Resurgent/RXDK-DotNet/releases/latest) release:

| File | What it is |
|---|---|
| `RxdkMonoHost.iso` | Bootable disc. This is the file xemu runs. |
| `RxdkMonoHost.xbe` | The title inside that ISO. |
| `rxdk-dotnet-runtime.zip` | `assy/` holds `mscorlib.dll`, `System.dll`, `System.Core.dll`, and `Test.dll`. `lib/` holds the native runtime archives. |

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

Each zip has `tools/` (`xbcp`, `xbset`, `xdvdfs`, `imagebld`, `xbox-launch`, …) and a .NET 8
runtime installer (`install-dotnet-runtime.cmd` or `install-dotnet-runtime.sh`). Compiling C#
uses the .NET SDK you already have; those tools are only for packing and for talking to a kit.

## Compile an app

The running title loads `D:\assy\Test.dll` and calls `RxdkTest.RunAll()`. `RunAll` returns the
number of failures. `Console.WriteLine` and `RxdkConsole.Write` both show up on the debug output.

Unzip `rxdk-dotnet-runtime.zip` and compile against the assemblies in `assy/`. This is not a
`dotnet build` of a normal SDK project: reference this `mscorlib.dll` with `-nostdlib`.

`App.cs`:

```csharp
using System;
using System.Runtime.CompilerServices;

public static class RxdkConsole
{
    [MethodImpl(MethodImplOptions.InternalCall)]
    public static extern void Write(string s);
}

public static class RxdkTest
{
    public static int RunAll()
    {
        RxdkConsole.Write("hello from the Xbox\n");
        Console.WriteLine("console works too");
        return 0;
    }
}
```

Point `csc.dll` at the SDK directory from `dotnet --list-sdks` (the path under your dotnet
install, for example `C:\Program Files\dotnet\sdk\10.0.400` on Windows). From the folder that
contains `assy/`:

```bash
dotnet exec "$CSC" -nostdlib -noconfig -optimize+ -unsafe \
  -reference:assy/mscorlib.dll -reference:assy/System.dll \
  -out:assy/Test.dll App.cs
```

Add `-reference:assy/System.Core.dll` if the app uses `System.Linq`. Sockets, DNS, and
`System.Net.Sockets.Socket` live in `System.dll`. `DeflateStream` is not in that assembly; the
self-test compiles it into `Test.dll` from Mono's sources.

After `RunAll` returns, the title also runs any `mini-*.dll` files sitting in `assy/`. Delete
those if you only want your program. `mscorlib.dll` has to stay in both the title root and
`assy/`.

## Copy it to the Xbox

`D:\` is the title drive: the DVD when you boot the ISO, or the title directory when you launch
from the hard disk. It is read-only from disc. `T:\` is the title's persistent partition and is
writable (`File.WriteAllText(@"T:\notes.txt", "...")`).

### xemu

Replace `assy/Test.dll` inside the disc tree and pack a new ISO with `xdvdfs` from the tools zip:

```text
RxdkMonoHost/
  default.xbe
  mscorlib.dll
  assy/
    mscorlib.dll
    System.dll
    System.Core.dll
    Test.dll
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
xbcp /y /t assy xE:\devkit\RxdkMonoHost\assy
xbcp /y mscorlib.dll xE:\devkit\RxdkMonoHost\mscorlib.dll
xbox-launch /dir xE:\devkit\RxdkMonoHost /title default.xbe
```

`/x <ip-or-name>` on any of those commands overrides the default kit. Debug text goes to
`xbwatson`.

## What an app can use

Verified on xemu: arithmetic, strings, arrays, generics (`List<T>`), delegates, virtual calls,
exceptions, `DateTime.Now`, `Guid.NewGuid`, `CultureInfo` (`new CultureInfo("en-US")`;
`CurrentCulture` stays invariant), file reads on `D:\`, file writes on `T:\`,
`Directory.GetFiles`, `DeflateStream` / `GZipStream` when those types are in your assembly,
UDP and TCP through `System.Net.Sockets.Socket`, and DNS (`Dns.GetHostName()` is `xbox`).

`127.0.0.1` is not a bindable address. Bind `0.0.0.0` and send to the title's own IPv4. The
serial log prints that address as it comes up (`RXDK-DotNet: net 192.168.1.96 dhcp 0x68`).
Binding `127.0.0.1` throws `SocketException` 10049.

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
bash scripts/build-testasm.sh
bash scripts/build-minitests.sh
bash scripts/build-host.sh
```

The ISO is `build-out/obj/host/RxdkMonoHost.iso`. Port notes, the Pentium III instruction limit,
and the PAL layout are in [`docs/port-plan.md`](docs/port-plan.md).
