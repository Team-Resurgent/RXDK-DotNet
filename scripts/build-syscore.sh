#!/usr/bin/env bash
# RXDK-DotNet — build a minimal System.Core.dll providing System.Linq (LINQ-to-objects) so the
# official mini generics.cs test (which does `using System.Linq`) can be built and run. We compile
# only corefx's System.Linq/*.cs (Enumerable/Where/Select/OrderBy/...) against our mscorlib — NOT
# the heavy System.Linq.Expressions/DLR that full System.Core pulls in. Output: build-out/corlib/System.Core.dll.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"; mkdir -p "$OUT"
CORLIB="$OUT/mscorlib.dll"
LINQ="$ROOT/vendor/mono/external/corefx/src/System.Linq/src/System/Linq"
# HashSet<T> lives in System.Core on net_4_x; System.Linq's ToHashSet needs it. ISet<T> (its
# interface) isn't in our mscorlib subset, so pull the corefx definition in here too.
CFX="$ROOT/vendor/mono/external/corefx/src"
ISET="$CFX/System.Runtime/src/System/Collections/Generic/ISet.cs"
SHIMS="$ROOT/build/managed/syscore-shims.cs"
ECMA="$ROOT/vendor/mono/mcs/class/ecma.pub"
# Internal corefx helpers System.Linq depends on (same files mono's common_System.Core.dll.sources
# pulls, minus the Expressions/Parallel blocks we don't need).
SUPPORT=(
  "$CFX/Common/src/System/Collections/Generic/ArrayBuilder.cs"
  "$CFX/Common/src/System/Collections/Generic/EnumerableHelpers.cs"
  "$CFX/Common/src/System/Collections/Generic/EnumerableHelpers.Linq.cs"
  "$CFX/Common/src/System/Collections/Generic/LargeArrayBuilder.cs"
  "$CFX/Common/src/System/Collections/Generic/SparseArrayBuilder.cs"
  "$CFX/System.Collections/src/System/Collections/Generic/BitHelper.cs"
  "$CFX/System.Collections/src/System/Collections/Generic/HashSet.cs"
  "$CFX/System.Collections/src/System/Collections/Generic/HashSetEqualityComparer.cs"
  "$CFX/System.Collections/src/System/Collections/Generic/ICollectionDebugView.cs"
  "$CFX/System.Collections/src/System/Collections/Generic/SortedList.cs"
)

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }

RSP="$OUT/syscore.rsp"
{
  echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-optimize+"; echo "-unsafe"
  echo "-runtimemetadataversion:v4.0.30319"
  # Delay-sign with the ECMA key so our identity is "System.Core, PublicKey=<ecma>" — mscorlib's
  # [InternalsVisibleTo("System.Core, PublicKey=...")] then grants HashSet its internal-member access.
  echo "-delaysign+"
  echo "-keyfile:$(cygpath -w "$ECMA")"
  echo "-nowarn:0169,0649,0067,0219,0414,3021,1685,0612,0618,0809,0108,0114,1591,0693,0436"
  # corefx System.Linq builds with these
  echo "-define:NET_4_0;NET_4_5;NET_4_6;MONO;FEATURE_CORECLR"
  echo "-out:$(cygpath -w "$OUT/System.Core.dll")"
  echo "-reference:$(cygpath -w "$CORLIB")"
  echo "$(cygpath -w "$SHIMS")"
  echo "$(cygpath -w "$ISET")"
  for f in "${SUPPORT[@]}"; do cygpath -w "$f"; done
  for f in "$LINQ"/*.cs; do cygpath -w "$f"; done
} > "$RSP"
echo "sources: $(grep -c '\.cs"\?$' "$RSP" | tr -d ' ') files"

echo "== compiling System.Core.dll (System.Linq) with Roslyn =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$OUT/syscore.log" 2>&1
rc=$?
echo "csc exit $rc"
echo "errors: $(grep -cE ': error CS' "$OUT/syscore.log")"
echo "-- top error categories --"
grep -oE 'error CS[0-9]+' "$OUT/syscore.log" | sort | uniq -c | sort -rn | head -12
echo "-- sample errors --"
grep -E ': error CS' "$OUT/syscore.log" | head -12
[ -f "$OUT/System.Core.dll" ] && echo "PRODUCED System.Core.dll ($(stat -c%s "$OUT/System.Core.dll") bytes)"
exit $rc
