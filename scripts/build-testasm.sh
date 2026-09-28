#!/usr/bin/env bash
# RXDK-DotNet — build the minimal managed test assembly (tests/managed/Test.cs) with Roslyn,
# referencing our own classic-Mono mscorlib.dll (/nostdlib). Output: build-out/corlib/Test.dll,
# bundled onto the DVD next to mscorlib so the interpreter can load and run it.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSC="/c/Program Files/dotnet/sdk/10.0.400/Roslyn/bincore/csc.dll"
OUT="$ROOT/build-out/corlib"
SRC="$ROOT/tests/managed/Test.cs"
CORLIB="$OUT/mscorlib.dll"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }

RSP="$OUT/testasm.rsp"
{
  echo "-nostdlib"
  echo "-noconfig"
  echo "-target:library"
  echo "-optimize+"
  echo "-out:$(cygpath -w "$OUT/Test.dll")"
  echo "-reference:$(cygpath -w "$CORLIB")"
  echo "$(cygpath -w "$SRC")"
} > "$RSP"

echo "== compiling Test.dll with Roslyn =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$OUT/testasm.log" 2>&1
rc=$?
echo "csc exit $rc"
[ "$rc" = "0" ] && ls -la "$OUT/Test.dll" | awk '{print "Test.dll:", $5, "bytes"}' || { echo "-- errors --"; grep -iE 'error' "$OUT/testasm.log" | head; }
exit $rc
