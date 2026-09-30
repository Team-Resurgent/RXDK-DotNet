#!/usr/bin/env bash
# RXDK-DotNet — build MonoGame.Framework.dll for the Xbox from vendor/monogame.
#
# MonoGame's own csprojs are net8.0 and pull NuGet (SharpDX, StbImageSharp, WindowsForms), none of
# which exists here. The split they rely on is still useful though: everything outside
# MonoGame.Framework/Platform is portable core, and each platform csproj adds a curated list of
# Platform/** files. So we glob the core the same way and add our own Xbox platform list.
#
# When the compile fails this reports the error magnitude and the dominant CS#### categories rather
# than the raw log, which is how the port was measured while the platform layer was being written.
# See docs/dotnet-version-gap.md.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
MG="$ROOT/vendor/monogame/MonoGame.Framework"
OUT="$ROOT/build-out/corlib"; mkdir -p "$OUT"
LOG="$ROOT/build-out/monogame.log"
CORLIB="$OUT/mscorlib.dll"

[ -d "$MG" ] || { echo "ERROR: vendor/monogame not checked out — git submodule update --init vendor/monogame"; exit 1; }
[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
STB="$ROOT/vendor/monogame/ThirdParty"
for s in StbImageSharp StbImageWriteSharp; do
  [ -d "$STB/$s/src" ] || { echo "ERROR: $s missing — git -C vendor/monogame submodule update --init ThirdParty/$s"; exit 1; }
done

# Directories that are never part of an Xbox build:
#   Platform/                 per-platform code; ours is listed explicitly below
#   Properties/               NuGet/assembly packaging attributes
#   Devices/                  phone sensors (Accelerometer, Compass)
#   Design/                   System.ComponentModel TypeConverters for the WinForms designer
#   Utilities/System.Numerics.Vectors/  vendored copy; our corlib already has System.Numerics
EXCLUDE_DIRS='/(bin|obj)/|/Platform/|/Properties/|/Devices/|/Design/|/Utilities/System\.Numerics\.Vectors/'

CORE="$OUT/monogame-core.txt"
find "$MG" -name '*.cs' -type f | sed 's|\\|/|g' | grep -Ev "$EXCLUDE_DIRS" | sort > "$CORE"

# The platform layer, in two parts.
#
# MonoGame ships a *.Default.cs for every platform service a backend may not have. The console has
# no mouse, no video playback, and no media library, so those are upstream's files unchanged.
UPSTREAM=(
  "Platform/Graphics/GraphicsDebug.Default.cs"
  "Platform/Audio/Microphone.Default.cs"
  "Platform/Audio/Xact/WaveBank.Default.cs"
  "Platform/Input/Joystick.Default.cs"
  "Platform/Input/KeyboardInput.Default.cs"
  "Platform/Input/MessageBox.Default.cs"
  "Platform/Input/Mouse.Default.cs"
  "Platform/Input/MouseCursor.Default.cs"
  "Platform/Input/KeysHelper.cs"
  "Platform/Input/InputKeyEventArgs.cs"
  "Platform/Input/Touch/TouchQueue.cs"
  "Platform/Media/MediaLibrary.Default.cs"
  # MediaPlayer.Default is a real implementation over the song queue, not a stub. Video and
  # VideoPlayer genuinely do throw, which is the honest answer: there is no video decoder.
  "Platform/Media/MediaPlayer.Default.cs"
  "Platform/Media/Video.Default.cs"
  "Platform/Media/VideoPlayer.Default.cs"
  "Platform/Threading.cs"
  "Platform/Utilities/AssemblyHelper.cs"
  "Platform/Utilities/ReflectionHelpers.Default.cs"
  # Texture2D.FromStream and SaveAsPng/SaveAsJpeg over StbImageSharp, as on DesktopGL. The decoder
  # itself is the ThirdParty submodules, added below.
  "Platform/Graphics/Texture2D.StbSharp.cs"
)

# Ours. Files that do not exist yet are skipped, so this script stays runnable through the whole
# port instead of only working at the end.
PLATFORM=(
  "Platform/Xbox/GamePlatform.Xbox.cs"
  "Platform/Xbox/GraphicsDeviceManager.Xbox.cs"
  "Platform/Xbox/XboxGameWindow.cs"
  "Platform/Xbox/TitleContainer.Xbox.cs"
  "Platform/Xbox/PlatformInfo.Xbox.cs"
  "Platform/Xbox/Graphics/XboxFormat.cs"
  "Platform/Xbox/Graphics/XboxRenderTargets.cs"
  "Platform/Xbox/Graphics/GraphicsAdapter.Xbox.cs"
  "Platform/Xbox/Graphics/GraphicsCapabilities.Xbox.cs"
  "Platform/Xbox/Graphics/GraphicsDevice.Xbox.cs"
  "Platform/Xbox/Graphics/OcclusionQuery.Xbox.cs"
  "Platform/Xbox/Graphics/Texture.Xbox.cs"
  "Platform/Xbox/Graphics/Texture2D.Xbox.cs"
  "Platform/Xbox/Graphics/Texture3D.Xbox.cs"
  "Platform/Xbox/Graphics/TextureCube.Xbox.cs"
  "Platform/Xbox/Graphics/TextureCollection.Xbox.cs"
  "Platform/Xbox/Graphics/RenderTarget2D.Xbox.cs"
  "Platform/Xbox/Graphics/RenderTarget3D.Xbox.cs"
  "Platform/Xbox/Graphics/RenderTargetCube.Xbox.cs"
  "Platform/Xbox/Graphics/SamplerStateCollection.Xbox.cs"
  "Platform/Xbox/Graphics/States/BlendState.Xbox.cs"
  "Platform/Xbox/Graphics/States/DepthStencilState.Xbox.cs"
  "Platform/Xbox/Graphics/States/RasterizerState.Xbox.cs"
  "Platform/Xbox/Graphics/States/SamplerState.Xbox.cs"
  "Platform/Xbox/Graphics/Shader/Shader.Xbox.cs"
  "Platform/Xbox/Graphics/Shader/ConstantBuffer.Xbox.cs"
  "Platform/Xbox/Graphics/Effect/EffectResource.Xbox.cs"
  "Platform/Xbox/Graphics/Vertices/VertexBuffer.Xbox.cs"
  "Platform/Xbox/Graphics/Vertices/IndexBuffer.Xbox.cs"
  "Platform/Xbox/Graphics/Vertices/VertexDeclaration.Xbox.cs"
  "Platform/Xbox/Input/GamePad.Xbox.cs"
  "Platform/Xbox/Input/Keyboard.Xbox.cs"
  "Platform/Xbox/Audio/XboxAudio.cs"
  "Platform/Xbox/Audio/SoundEffect.Xbox.cs"
  "Platform/Xbox/Audio/SoundEffectInstance.Xbox.cs"
  "Platform/Xbox/Audio/DynamicSoundEffectInstance.Xbox.cs"
  "Platform/Xbox/Media/Song.Xbox.cs"
)

# The stock effects. MonoGame builds these from HLSL with mgfxc; nothing on a PC can produce NV2A
# shaders, so ours are hand-written assembly the console assembles on first use. See the generator.
EFFECTS="$OUT/effects"
PY="${PYTHON:-python}"
"$PY" "$ROOT/tools/mgfx-xbox.py" "$EFFECTS" || exit 1

RSP="$OUT/monogame.rsp"
{
  echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-optimize+"; echo "-unsafe"
  echo "-langversion:latest"
  echo "-runtimemetadataversion:v4.0.30319"
  echo "-out:$(cygpath -w "$OUT/MonoGame.Framework.dll")"
  echo "-reference:$(cygpath -w "$CORLIB")"
  echo "-reference:$(cygpath -w "$OUT/System.dll")"
  echo "-reference:$(cygpath -w "$OUT/System.Core.dll")"
  # System.Numerics.Vectors only forwards Vector2, Matrix4x4, and the rest to System.Numerics.
  for r in System.Numerics System.Numerics.Vectors System.Runtime.Serialization; do
    [ -f "$OUT/$r.dll" ] || { echo "ERROR: $OUT/$r.dll missing — run scripts/build-bcl.sh first" >&2; exit 1; }
    echo "-reference:$(cygpath -w "$OUT/$r.dll")"
  done
  for r in Rxdk.Graphics Rxdk.Input Rxdk.Kernel; do
    [ -f "$OUT/$r.dll" ] && echo "-reference:$(cygpath -w "$OUT/$r.dll")"
  done
  [ -f "$OUT/Rxdk.Audio.dll" ] || { echo "ERROR: $OUT/Rxdk.Audio.dll missing — run scripts/build-sdklibs.sh first" >&2; exit 1; }
  echo "-reference:$(cygpath -w "$OUT/Rxdk.Audio.dll")"
  # XBOX drives our own #if blocks. MonoGame's XNADESIGNPROVIDED is deliberately absent: it turns on
  # the Design/ TypeConverters, which need System.ComponentModel.
  #
  # NET45 is not a guess: MonoGame uses it to pick the .NET Framework 4.5 spellings of the
  # reflection and Enum APIs, and our corlib is mono's net_4_x profile, so those are the ones we
  # have. Without it the framework calls Enum.GetValues<T>() and TypeInfo members from .NET 5.
  # STBSHARP_INTERNAL keeps StbImageSharp's types internal, as DesktopGL builds it.
  echo "-define:XBOX;NET45;STBSHARP_INTERNAL"
  # Doc comments, unused fields, obsolete members, and unreachable code are upstream's business.
  echo "-nowarn:0169,0649,0067,0219,0414,1591,0618,0612,0162,0108,0114,0067,1685,0693"
  # Resource names must match the constants in Platform/Xbox/Graphics/Effect/EffectResource.Xbox.cs.
  for e in "$EFFECTS"/*.xbox.mgfxo; do
    echo "-resource:$(cygpath -w "$e"),Microsoft.Xna.Framework.Platform.Xbox.Graphics.Effect.Resources.$(basename "$e")"
  done
  while read -r f; do cygpath -w "$f"; done < "$CORE"
  find "$STB/StbImageSharp/src" "$STB/StbImageWriteSharp/src" -name '*.cs' -type f | sort \
    | while read -r f; do cygpath -w "$f"; done
  for p in "${UPSTREAM[@]}" "${PLATFORM[@]}"; do
    [ -f "$MG/$p" ] && cygpath -w "$MG/$p"
  done
} > "$RSP"

core_n=$(wc -l < "$CORE" | tr -d ' ')
up_n=0; plat_n=0
for p in "${UPSTREAM[@]}"; do
  [ -f "$MG/$p" ] && up_n=$((up_n + 1)) || echo "  WARNING: upstream file gone: $p"
done
for p in "${PLATFORM[@]}"; do [ -f "$MG/$p" ] && plat_n=$((plat_n + 1)); done
echo "sources: $core_n core + $up_n upstream default + $plat_n xbox"

echo "== compiling MonoGame.Framework.dll with Roslyn =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$LOG" 2>&1
rc=$?
errs=$(grep -cE ': error CS' "$LOG")
echo "csc exit $rc"
echo "errors: $errs"
if [ "$errs" -gt 0 ]; then
  echo "-- top error categories --"
  grep -oE 'error CS[0-9]+' "$LOG" | sort | uniq -c | sort -rn | head -15
  echo "-- most affected files --"
  grep -E ': error CS' "$LOG" | sed 's|^.*MonoGame.Framework\\||; s|(.*||' | sort | uniq -c | sort -rn | head -15
  echo "-- missing types (CS0246/CS0234/CS0103) --"
  grep -oE "error CS(0246|0234|0103): The type or namespace name '[^']+'" "$LOG" \
    | grep -oE "'[^']+'" | sort | uniq -c | sort -rn | head -20
  echo "full log: build-out/monogame.log"
fi
[ -f "$OUT/MonoGame.Framework.dll" ] && echo "PRODUCED MonoGame.Framework.dll ($(stat -c%s "$OUT/MonoGame.Framework.dll") bytes)"
exit $rc
