#!/usr/bin/env bash
# RXDK-DotNet — the class libraries beyond mscorlib, each built complete from Mono's own sources.
#
# Every assembly here compiles exactly the file list Mono's build would for the win32 net_4_x
# profile, the same profile scripts/build-corlib.sh builds mscorlib from, so identities (4.0.0.0,
# the .NET Framework public keys) and defines match. scripts/gensources.py resolves the lists. The
# references, defines, keys, and resources for each library come from its Makefile under
# vendor/mono/mcs/class; the comment above each one names anything that differs.
#
# Mono breaks the System <-> System.Xml <-> System.Configuration cycle the way its own build does:
# a library compiles against the .NET Framework 4.7.1 reference assemblies (API_BIN_REFS in the
# Makefiles) for anything not yet built.
#
# Output: build-out/corlib/<assembly>.dll
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"; mkdir -p "$OUT"
CORLIB="$OUT/mscorlib.dll"
CLASS="$ROOT/vendor/mono/mcs/class"
API="$ROOT/vendor/mono/external/binary-reference-assemblies/v4.7.1"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
[ -d "$ROOT/vendor/mono/external/corefx/src" ] || { echo "ERROR: corefx sources missing — git -C vendor/mono submodule update --init external/corefx"; exit 1; }
[ -d "$API" ] || { echo "ERROR: reference assemblies missing — git -C vendor/mono submodule update --init --depth 1 external/binary-reference-assemblies"; exit 1; }

# mcs/build/profiles/net_4_x.make
PROFILE_DEFINES="NET_4_0;NET_4_5;NET_4_6;MONO;WIN_PLATFORM"

fail=0

# mono_lib <class dir> <assembly> <key> — then optional settings, each a separate word:
#   ref:Name         reference build-out/corlib/Name.dll (ref:Alias=Name for an extern alias)
#   api:Name         reference the 4.7.1 reference assembly for Name
#   -anything        passed to csc as is (defines, -resource:, -nowarn:)
# Relative -resource: paths are relative to the class dir, as in the Makefile.
mono_lib() {
  local dir="$CLASS/$1" asm="$2" key="$3"; shift 3
  local list="$OUT/${asm%.dll}.sources.txt"
  python "$ROOT/scripts/gensources.py" "$dir" "$asm" win32 net_4_x > "$list" || { fail=$((fail + 1)); return; }
  compile_lib "$dir" "$asm" "$key" "$list" "$@"
}

# compile_lib <dir> <assembly> <key> <source list> — the settings are mono_lib's. For a library
# whose file list is not one Mono's build resolves. LIB_DEFINES replaces the profile defines.
compile_lib() {
  local dir="$1" asm="$2" key="$3" list="$4"; shift 4
  local name="${asm%.dll}"
  local rsp="$OUT/$name.rsp" log="$OUT/$name.log"
  {
    echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-optimize+"; echo "-deterministic"
    # C# 14 made `field` a keyword inside property accessors, which the reference source uses as
    # an identifier.
    echo "-langversion:13"
    echo "-runtimemetadataversion:v4.0.30319"
    echo "-delaysign+"
    echo "-keyfile:$(cygpath -w "$CLASS/$key")"
    # 1616: the key named here overrides the AssemblyKeyFile attribute some AssemblyInfo.cs carry.
    echo "-nowarn:1699,1616,0169,0649,0067,0219,0414,0618,0612,0162,0168,0436,1591,3021,0809,0693,0108,0114,1717,1701,1702"
    local defines="${LIB_DEFINES-$PROFILE_DEFINES}"
    [ -n "$defines" ] && echo "-define:$defines"
    echo "-out:$(cygpath -w "$OUT/$asm")"
    echo "-reference:$(cygpath -w "$CORLIB")"
    local a
    for a in "$@"; do
      case "$a" in
        ref:*=*) local alias="${a#ref:}"; echo "-reference:${alias%%=*}=$(cygpath -w "$OUT/${alias#*=}.dll")" ;;
        ref:*)   echo "-reference:$(cygpath -w "$OUT/${a#ref:}.dll")" ;;
        api:*)   echo "-reference:$(cygpath -w "$API/${a#api:}.dll")" ;;
        -resource:*) local r="${a#-resource:}"; [ "${r#/}" = "$r" ] && r="$dir/$r"; echo "-resource:$(cygpath -w "$r")" ;;
        *)       echo "$a" ;;
      esac
    done
    tr -d '\r' < "$list"
  } > "$rsp"
  echo "== $name ($(wc -l < "$list" | tr -d ' ') sources) =="
  (cd "$dir" && MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$rsp")") > "$log" 2>&1
  if [ $? -ne 0 ]; then
    echo "  FAILED ($(grep -cE ': error CS' "$log") errors) — $log"
    grep -oE 'error CS[0-9]+' "$log" | sort | uniq -c | sort -rn | head -5
    grep -E ': error CS' "$log" | head -5
    fail=$((fail + 1))
  else
    echo "  ok ($(stat -c%s "$OUT/$asm") bytes)"
    echo "$asm" >> "$MANIFEST"
  fi
}

# The assemblies this script built, one per line, for scripts that stage them onto a disc.
MANIFEST="$OUT/classlibs.txt"
: > "$MANIFEST"

REFSRC_SYSTEM="-define:FEATURE_PAL;SYSTEM_NAMESPACE;MONO;PLATFORM_UNIX;MONO_FEATURE_PROCESS_START;MONO_FEATURE_THREAD_ABORT;MONO_FEATURE_THREAD_SUSPEND_RESUME;MONO_FEATURE_MULTIPLE_APPDOMAINS"

mono_lib Mono.Security Mono.Security.dll mono.pub api:System -unsafe -nowarn:1030,3009

# System's Makefile also gives System.Net.Http, which is built after it, as an API reference.
mono_lib System System.dll ecma.pub \
  ref:MonoSecurity=Mono.Security \
  api:System.Net.Http api:System.Xml api:System.Core api:System.Numerics api:System.Configuration \
  -define:COREFX -define:CONFIGURATION_2_0 -define:SYSTEM_NET_PRIMITIVES_DLL -define:XML_DEP -define:SECURITY_DEP \
  -define:MONO_SECURITY_ALIAS -define:CODEDOM -define:CONFIGURATION_DEP -define:FEATURE_COMPILED \
  "$REFSRC_SYSTEM" -unsafe \
  -resource:resources/Asterisk.wav -resource:resources/Beep.wav -resource:resources/Exclamation.wav \
  -resource:resources/Hand.wav -resource:resources/Question.wav

mono_lib System.XML System.Xml.dll ecma.pub ref:System api:System.Configuration \
  -unsafe -define:ASYNC -define:CONFIGURATION_DEP

mono_lib System.Core System.Core.dll ecma.pub ref:System \
  -define:FEATURE_PAL -define:PFX_LEGACY_3_5 -define:FEATURE_NETCORE -define:INSIDE_SYSCORE -define:LIBC \
  -define:NET_3_5 -define:FEATURE_COMPILE -define:FEATURE_COMPILE_TO_METHODBUILDER -unsafe

mono_lib System.Numerics System.Numerics.dll ecma.pub ref:System -unsafe

mono_lib System.Security System.Security.dll msfinal.pub \
  ref:Mono.Security ref:System ref:System.Xml api:System.Numerics api:System.Core \
  -unsafe -define:SECURITY_DEP

mono_lib System.Configuration System.Configuration.dll msfinal.pub ref:System.Security ref:System ref:System.Xml

mono_lib System.Numerics.Vectors System.Numerics.Vectors.dll msfinal.pub ref:System ref:System.Numerics -unsafe

mono_lib System.Xml.Linq System.Xml.Linq.dll ecma.pub ref:System ref:System.Core ref:System.Xml -unsafe

mono_lib System.Transactions System.Transactions.dll ecma.pub ref:System ref:System.Configuration

mono_lib System.EnterpriseServices System.EnterpriseServices.dll msfinal.pub ref:System.Transactions

mono_lib System.ServiceModel.Internals System.ServiceModel.Internals.dll ecma.pub \
  ref:System ref:System.Core ref:System.Xml -unsafe "$REFSRC_SYSTEM"

mono_lib SMDiagnostics SMDiagnostics.dll ecma.pub \
  ref:System ref:System.Core ref:System.Xml ref:System.ServiceModel.Internals ref:System.Configuration \
  -define:NO_CONFIGURATION

mono_lib System.Data System.Data.dll ecma.pub \
  ref:System ref:System.Xml ref:System.Core ref:System.Numerics ref:System.Transactions \
  ref:System.EnterpriseServices ref:System.Configuration ref:Mono.Security \
  -define:COREFX -define:PLATFORM_UNIX -define:USEOFFSET -define:MONO_PARTIAL_DATA_IMPORT -unsafe \
  -resource:../../../external/corefx/src/System.Data.SqlClient/src/Resources/System.Data.SqlClient.SqlMetaData.xml

mono_lib System.Runtime.Serialization System.Runtime.Serialization.dll ecma.pub \
  ref:System ref:System.Xml ref:System.Core ref:System.ServiceModel.Internals \
  ref:System.Data ref:System.Configuration ref:SMDiagnostics \
  -unsafe -define:NO_DYNAMIC_CODEGEN -define:NET_3_0

mono_lib System.IO.Compression System.IO.Compression.dll ecma.pub ref:System -unsafe

mono_lib System.IO.Compression.FileSystem System.IO.Compression.FileSystem.dll ecma.pub \
  ref:System ref:System.IO.Compression -unsafe

mono_lib System.ComponentModel.DataAnnotations System.ComponentModel.DataAnnotations.dll winfx.pub \
  ref:System ref:System.Core ref:System.Xml

# RESOURCE_DEFS: the Strings class reads Microsoft.Internal.Strings.resources, compiled from its .resx.
COMPOSITION_RES="$OUT/Microsoft.Internal.Strings.resources"
dotnet msbuild "$(cygpath -w "$ROOT/build/tools/resgen.proj")" -nologo -v:q \
  "-p:Resx=$(cygpath -w "$CLASS/System.ComponentModel.Composition.4.5/src/ComponentModel/Strings.resx")" \
  "-p:Resources=$(cygpath -w "$COMPOSITION_RES")" > "$OUT/resgen.log" 2>&1 \
  || { echo "ERROR: resgen failed — $OUT/resgen.log"; exit 1; }
mono_lib System.ComponentModel.Composition.4.5 System.ComponentModel.Composition.dll ecma.pub \
  ref:System ref:System.Core \
  -define:CLR40 -define:USE_ECMA_KEY -define:FEATURE_REFLECTIONCONTEXT -define:FEATURE_REFLECTIONFILEIO \
  -define:FEATURE_SERIALIZATION -define:FEATURE_SLIMLOCK "-resource:$COMPOSITION_RES"

mono_lib System.Json System.Json.dll winfx.pub ref:System ref:System.Xml ref:System.Core

mono_lib System.Net.Http System.Net.Http.dll msfinal.pub ref:System.Core ref:System -unsafe -nowarn:436

# Mono's net_4_x System.Drawing.Primitives is a facade over System.Drawing, whose GDI+ half needs
# libgdiplus. corefx's own project is the managed half (Color, Point, Rectangle, Size), so this is
# its file list, with the facade's identity. It gets none of the profile defines: MONO adds Mono's
# System.Drawing SystemColors, and FEATURE_WINDOWS_SYSTEM_COLORS reads system colors from user32.
# Its SR constants come from its .resx, as Mono's resx2sr would make them.
CFX="$ROOT/vendor/mono/external/corefx/src"
PRIM_LIST="$OUT/System.Drawing.Primitives.sources.txt"
PRIM_SR="$OUT/System.Drawing.Primitives.SR.cs"
python - "$CFX/System.Drawing.Primitives/src/Resources/Strings.resx" "$PRIM_SR" <<'EOF' || fail=$((fail + 1))
import sys, xml.etree.ElementTree as ET
data = ET.parse(sys.argv[1]).getroot().findall("data")
with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("partial class SR\n{\n")
    for d in data:
        value = d.find("value").text.replace('"', '""')
        f.write('\tpublic const string %s = @"%s";\n' % (d.get("name"), value))
    f.write("}\n")
EOF
{
  for f in "$CLASS/Facades/System.Drawing.Primitives/AssemblyInfo.cs" "$ROOT/vendor/mono/mcs/build/common/SR.cs" "$PRIM_SR" \
    "$CFX"/System.Drawing.Primitives/src/System/Drawing/{Point,PointF,Rectangle,RectangleF,Size,SizeF,Color}.cs \
    "$CFX"/Common/src/System/Drawing/{ColorTable,ColorUtil.netcoreapp21,KnownColor,KnownColorTable}.cs \
    "$CFX/Common/src/System/Numerics/Hashing/HashHelpers.cs"; do
    cygpath -w "$f"
  done
} > "$PRIM_LIST"
LIB_DEFINES= compile_lib "$CFX/System.Drawing.Primitives/src" System.Drawing.Primitives.dll msfinal.pub "$PRIM_LIST" ref:System

mono_lib System.Reflection.Context System.Reflection.Context.dll ecma.pub ref:System

# System.Runtime.CompilerServices.Unsafe is IL, not C#; its Makefile assembles corlib's Unsafe.il.
echo "== System.Runtime.CompilerServices.Unsafe =="
mono_build_tools || exit 1
UNSAFE_DIR="$CLASS/System.Runtime.CompilerServices.Unsafe"
MSYS2_ARG_CONV_EXCL='*' "$ILASM" "$(cygpath -w "$UNSAFE_DIR/AssemblyInfo.il")" \
  "$(cygpath -w "$CLASS/corlib/System.Runtime.CompilerServices/Unsafe.il")" -dll -quiet \
  "-output=$(cygpath -w "$OUT/System.Runtime.CompilerServices.Unsafe.dll")" > "$OUT/System.Runtime.CompilerServices.Unsafe.log" 2>&1
if [ $? -ne 0 ]; then
  echo "  FAILED — $OUT/System.Runtime.CompilerServices.Unsafe.log"; fail=$((fail + 1))
else
  echo "  ok ($(stat -c%s "$OUT/System.Runtime.CompilerServices.Unsafe.dll") bytes)"
  echo "System.Runtime.CompilerServices.Unsafe.dll" >> "$MANIFEST"
fi

mono_lib I18N/Common I18N.dll mono.pub -unsafe -define:DISABLE_UNSAFE
for enc in West MidEast Other Rare; do
  mono_lib "I18N/$enc" "I18N.$enc.dll" mono.pub ref:I18N -unsafe
done
mono_lib I18N/CJK I18N.CJK.dll mono.pub ref:I18N -unsafe -define:DISABLE_UNSAFE \
  -resource:big5.table -resource:gb2312.table -resource:jis.table -resource:ks.table -resource:gb18030.table

if [ "$fail" -gt 0 ]; then
  echo "===== $fail assembly(ies) failed ====="
  exit 1
fi
echo "===== class libraries built ====="
