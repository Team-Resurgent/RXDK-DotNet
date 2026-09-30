#!/usr/bin/env bash
# RXDK-DotNet — build the Gum UI runtime for the Xbox from vendor/gum.
#
# Gum ships as NuGet packages: Gum.MonoGame (MonoGameGum.dll, net8.0) over FlatRedBall.GumCommon
# (GumCommon.dll, netstandard2.0) over FlatRedBall.InterpolationCore. None of them can be used as
# built, since they reference netstandard.dll or .NET 8, so each is compiled here against our
# class libraries and MonoGame.Framework.dll, the way scripts/build-monogame.sh builds MonoGame.
#
# The file lists are the ones upstream's csprojs produce, read from MSBuild's evaluation of them, so
# a Gum update needs no list maintenance here. Evaluation needs no restore. GumCommon is evaluated
# for netstandard2.0, MonoGameGum for net8.0; neither gets the NET*_OR_GREATER symbols, so both take
# Gum's netstandard2.0 code paths, which is the API level of the net_4_x libraries.
#
# Output: build-out/corlib/{FlatRedBall.InterpolationCore,GumCommon,MonoGameGum}.dll
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
GUM="$ROOT/vendor/gum"
INTERP="$ROOT/vendor/flatredball-interpolation"
OUT="$ROOT/build-out/corlib"
WORK="$ROOT/build-out/gum"; mkdir -p "$WORK"

[ -f "$GUM/MonoGameGum/MonoGameGum.csproj" ] || { echo "ERROR: vendor/gum not checked out — git submodule update --init vendor/gum"; exit 1; }
for r in mscorlib MonoGame.Framework System.Drawing.Primitives System.Net.Http; do
  [ -f "$OUT/$r.dll" ] || { echo "ERROR: $OUT/$r.dll missing — build the class libraries and MonoGame first"; exit 1; }
done

# items <csproj> <tfm> <item type> — full paths, one per line, excluding obj/ and bin/.
items() {
  local json="$WORK/$(basename "$1" .csproj).$3.json"
  dotnet msbuild "$(cygpath -w "$1")" -nologo "-getItem:$3" "-p:TargetFramework=$2" -p:Configuration=Release \
    > "$json" 2> "$WORK/msbuild.log" || { echo "ERROR: evaluating $1 failed — $WORK/msbuild.log" >&2; return 1; }
  python - "$json" "$3" <<'EOF'
import json, sys
items = json.load(open(sys.argv[1], encoding="utf-8-sig")).get("Items", {}).get(sys.argv[2], [])
for i in items:
    p = i["FullPath"]
    if "\\obj\\" not in p and "\\bin\\" not in p:
        print(p)
EOF
}

# compile <assembly> <defines> <references...> — sources on stdin, one Windows path per line.
compile() {
  local name="$1" defines="$2"; shift 2
  local rsp="$WORK/$name.rsp" log="$WORK/$name.log"
  {
    echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-optimize+"; echo "-unsafe"
    echo "-deterministic"
    # Gum's csprojs pin C# 12 and nullable annotations.
    echo "-langversion:12"; echo "-nullable:enable"
    echo "-runtimemetadataversion:v4.0.30319"
    echo "-out:$(cygpath -w "$OUT/$name.dll")"
    [ -n "$defines" ] && echo "-define:$defines"
    # Nullable analysis, doc comments, unused and obsolete members are upstream's business.
    echo "-nowarn:nullable,0169,0649,0067,0219,0414,1591,0618,0612,0162,0168,0108,0114,0693,1998,4014"
    local r
    for r in mscorlib System System.Core System.Numerics System.Numerics.Vectors System.Xml System.Xml.Linq \
      System.Runtime.Serialization System.Drawing.Primitives System.Net.Http "$@"; do
      echo "-reference:$(cygpath -w "$OUT/$r.dll")"
    done
    cat
  } > "$rsp"
  echo "== $name ($(grep -vc '^-' "$rsp") sources) =="
  MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$rsp")" > "$log" 2>&1
  if [ $? -ne 0 ]; then
    echo "  FAILED ($(grep -cE ': error CS' "$log") errors) — $log"
    grep -oE 'error CS[0-9]+' "$log" | sort | uniq -c | sort -rn | head -5
    grep -E ': error CS' "$log" | head -10
    exit 1
  fi
  echo "  ok ($(stat -c%s "$OUT/$name.dll") bytes)"
}

find "$INTERP" -name '*.cs' -type f | sort | while read -r f; do cygpath -w "$f"; done \
  | compile FlatRedBall.InterpolationCore ""

# Upstream's Release configuration for netstandard2.0.
items "$GUM/GumCommon/GumCommon.csproj" netstandard2.0 Compile > "$WORK/GumCommon.sources.txt" || exit 1
compile GumCommon "FULL_DIAGNOSTICS" FlatRedBall.InterpolationCore < "$WORK/GumCommon.sources.txt"

# Upstream's Release configuration for net8.0, plus XBOX. The embedded fonts and UI sprite sheet are
# named the way the SDK names them, RootNamespace plus the item's path with dots, which is what
# SystemManagers.LoadEmbedded* asks for.
items "$GUM/MonoGameGum/MonoGameGum.csproj" net8.0 Compile > "$WORK/MonoGameGum.sources.txt" || exit 1
items "$GUM/MonoGameGum/MonoGameGum.csproj" net8.0 EmbeddedResource > "$WORK/MonoGameGum.resources.txt" || exit 1
{
  tr -d '\r' < "$WORK/MonoGameGum.sources.txt"
  tr -d '\r' < "$WORK/MonoGameGum.resources.txt" | while read -r f; do
    base="$(basename "$(cygpath -u "$f")")"
    echo "-resource:$f,MonoGameGum.Content.$base"
  done
} | compile MonoGameGum "MONOGAME;USE_GUMCOMMON;FULL_DIAGNOSTICS;XBOX" \
  FlatRedBall.InterpolationCore GumCommon MonoGame.Framework
