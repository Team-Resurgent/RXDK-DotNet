#!/usr/bin/env bash
# RXDK-DotNet — the extra base class libraries a title needs beyond mscorlib and System.Core.
#
# Each of these is a real Mono/corefx class library compiled against our mscorlib, not a shim. They
# exist because third-party code expects them: MonoGame alone needs System.Numerics' vector types
# for its conversion operators, the DataContract attributes on every one of its math structs,
# Stopwatch for the game loop, and Regex for content loading. See docs/dotnet-version-gap.md for
# the rule this follows: when a type is missing, build the class library that owns it.
#
# Output: build-out/corlib/{System.Numerics.Vectors,System.Runtime.Serialization}.dll
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"; mkdir -p "$OUT"
CORLIB="$OUT/mscorlib.dll"
CFX="$ROOT/vendor/mono/external/corefx/src"
ECMA="$ROOT/vendor/mono/mcs/class/ecma.pub"
SHIMS="$ROOT/build/managed/bcl-shims.cs"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
[ -d "$CFX" ] || { echo "ERROR: corefx sources missing — git -C vendor/mono submodule update --init external/corefx"; exit 1; }

fail=0

# $1 = assembly name, $2 = extra defines, rest = source files
build_asm() {
  local name="$1"; shift
  local defines="$1"; shift
  local rsp="$OUT/$name.rsp"
  local log="$OUT/$name.log"
  {
    echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-optimize+"; echo "-unsafe"
    echo "-runtimemetadataversion:v4.0.30319"
    echo "-delaysign+"
    echo "-keyfile:$(cygpath -w "$ECMA")"
    echo "-nowarn:0169,0649,0067,0219,0414,1591,0618,0612,0162,3021,1685,0809,0693,0436,0108"
    echo "-define:NET_4_0;NET_4_5;NET_4_6;MONO;FEATURE_CORECLR;$defines"
    echo "-out:$(cygpath -w "$OUT/$name.dll")"
    echo "-reference:$(cygpath -w "$CORLIB")"
    local f
    for f in "$@"; do cygpath -w "$f"; done
  } > "$rsp"
  MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$rsp")" > "$log" 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then
    echo "  FAILED ($(grep -cE ': error CS' "$log") errors) — $log"
    grep -oE 'error CS[0-9]+' "$log" | sort | uniq -c | sort -rn | head -5
    grep -E ': error CS' "$log" | head -5
    fail=$((fail + 1))
  else
    echo "  ok ($(stat -c%s "$OUT/$name.dll") bytes, $# sources)"
  fi
}

# System.Numerics.Vectors — Vector2/3/4, Matrix3x2, Matrix4x4, Plane, Quaternion. mscorlib already
# has the generic Vector<T>, MathF, and HashHelpers these build on.
echo "== System.Numerics.Vectors =="
NUMERICS=("$SHIMS")
for f in "$CFX"/System.Numerics.Vectors/src/System/Numerics/*.cs; do NUMERICS+=("$f"); done
# [Intrinsic] is an internal attribute each corefx assembly declares for itself; corlib's copy is
# internal to corlib, so compile the shared definition in here too.
NUMERICS+=("$CFX/Common/src/CoreLib/System/Runtime/CompilerServices/IntrinsicAttribute.cs")
NUMERICS+=("$ROOT/vendor/mono/external/corert/src/Common/src/System/Numerics/Hashing/HashHelpers.cs")
build_asm System.Numerics.Vectors "" "${NUMERICS[@]}"

# System.Runtime.Serialization — the DataContract/DataMember attribute family only. The real
# assembly also carries DataContractSerializer; nothing here needs it, and the attributes are what
# third-party types are decorated with.
echo "== System.Runtime.Serialization =="
SER=("$SHIMS")
for f in "$CFX"/System.Runtime.Serialization.Primitives/src/System/Runtime/Serialization/*.cs; do SER+=("$f"); done
build_asm System.Runtime.Serialization "" "${SER[@]}"

# System.dll — was a two-type stub for the socket tests. Promote it to the real subset: Stopwatch
# (every game loop needs it), Uri, Regex (content loading parses paths with it), and the
# System.ComponentModel attributes. The socket helper stays because the native socket code loads
# "System.dll" by name and Test.cs binds to it.
echo "== System =="
# Regex throws with SR.<name>; generate that class from corefx's own string table rather than
# copying 47 messages by hand. bcl-shims.cs is deliberately not used here, it has its own SR.
SR_GEN="$OUT/SR.System.g.cs"
python "$ROOT/scripts/gen-sr.py" "$SR_GEN" \
  "$CFX/System.Text.RegularExpressions/src/Resources/Strings.resx" \
  "$CFX/System.Private.Uri/src/Resources/Strings.resx" || exit 1
SYSTEM=("$SR_GEN" "$ROOT/tests/managed/SystemNet.cs")
SYSTEM+=("$ROOT/vendor/mono/mcs/class/System/System.Diagnostics/Stopwatch.cs")
# The public Debug belongs to System.dll in this profile. Mono's corlib carries an internal copy it
# renames out of the way in a post-processing step we do not run, so without a public one here
# every Debug.Assert in third-party code fails with CS0122. See the file for why it is not the
# reference source version.
SYSTEM+=("$ROOT/build/managed/system-debug.cs")
SYSTEM+=("$CFX/Common/src/CoreLib/System/ComponentModel/DefaultValueAttribute.cs")
# The rest of System.ComponentModel that ordinary code touches, plus GeneratedCodeAttribute, which
# every .resx designer file carries. These are small standalone files in the reference source with no
# dependency on the TypeDescriptor stack, so they come in without dragging the designer in with them.
REFSRC="$ROOT/vendor/mono/mcs/class/referencesource/System/compmod/system"
SYSTEM+=("$REFSRC/componentmodel/EditorBrowsableAttribute.cs")
SYSTEM+=("$REFSRC/componentmodel/INotifyPropertyChanged.cs")
SYSTEM+=("$REFSRC/componentmodel/PropertyChangedEventArgs.cs")
SYSTEM+=("$REFSRC/componentmodel/PropertyChangedEventHandler.cs")
SYSTEM+=("$REFSRC/codedom/compiler/GeneratedCodeAttribute.cs")
# Regex: take mono's own file list rather than globbing corefx. It omits RegexCompiler, which needs
# System.Reflection.Emit, and adds the HashtableExtensions and ValueListBuilder the rest expect.
MONOSYS="$ROOT/vendor/mono/mcs/class/System"
missing=0
add_mono_source() {
  local rel="${1%$'\r'}"
  if [ -f "$MONOSYS/$rel" ]; then
    SYSTEM+=("$MONOSYS/$rel")
  else
    echo "  WARNING: source not on disk: $rel"
    missing=$((missing + 1))
  fi
}
# Uri comes from corefx, not from mono's list. Mono builds the .NET reference source URI.cs, which
# carries [TypeConverter(typeof(UriTypeConverter))] and so drags in the whole TypeDescriptor stack
# for an attribute only a visual designer reads. corefx's Uri is the same class without that.
uri_n=0
for f in "$CFX"/System.Private.Uri/src/System/*.cs; do
  [ -f "$f" ] || continue
  [ "$(basename "$f")" = "Uri.Unix.cs" ] && continue
  SYSTEM+=("$f"); uri_n=$((uri_n + 1))
done
[ "$uri_n" -eq 0 ] && { echo "  ERROR: no Uri sources found under $CFX/System.Private.Uri/src/System"; exit 1; }
# The IP address helpers are partial; their other halves live in corefx's shared Common tree.
SYSTEM+=("$CFX/Common/src/System/Net/IPv4AddressHelper.Common.cs")
SYSTEM+=("$CFX/Common/src/System/Net/IPv6AddressHelper.Common.cs")
while read -r rel; do add_mono_source "$rel"; done \
  < <(grep -E 'RegularExpressions|ValueListBuilder' "$MONOSYS/common.sources")
[ "$missing" -gt 0 ] && echo "  $missing source(s) missing"
build_asm System "" "${SYSTEM[@]}"

if [ "$fail" -gt 0 ]; then
  echo "===== $fail assembly(ies) failed ====="
  exit 1
fi
echo "===== extra BCL assemblies built ====="
