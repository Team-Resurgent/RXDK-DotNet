#!/usr/bin/env bash
# RXDK-DotNet — build the minimal managed test assembly (tests/managed/Test.cs) with Roslyn,
# referencing our own classic-Mono mscorlib.dll (/nostdlib). Output: build-out/corlib/Main.dll,
# bundled onto the DVD next to mscorlib so the interpreter can load and run it.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"
SRC="$ROOT/tests/managed/Test.cs"
CORLIB="$OUT/mscorlib.dll"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
[ -f "$OUT/Rxdk.Input.dll" ] || { echo "ERROR: $OUT/Rxdk.Input.dll not found — run scripts/build-input.sh first"; exit 1; }

# Minimal System.dll: SocketAddress (the native socket code loads "System.dll" by name) and a
# UDP loopback helper whose methods are the real Socket icalls.
SYS="$OUT/System.dll"
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" -nostdlib -noconfig -target:library -unsafe -optimize+ \
  -out:"$(cygpath -w "$SYS")" -reference:"$(cygpath -w "$CORLIB")" \
  "$(cygpath -w "$ROOT/tests/managed/SystemNet.cs")" > "$OUT/system.log" 2>&1 \
  || { echo "System.dll FAILED"; grep -iE 'error' "$OUT/system.log" | head; exit 1; }
echo "System.dll: $(stat -c%s "$SYS" 2>/dev/null) bytes"

RSP="$OUT/testasm.rsp"
{
  echo "-nostdlib"
  echo "-noconfig"
  echo "-target:exe"
  echo "-optimize+"
  echo "-unsafe"
  echo "-out:$(cygpath -w "$OUT/Main.dll")"
  echo "-reference:$(cygpath -w "$CORLIB")"
  echo "-reference:$(cygpath -w "$OUT/System.dll")"
  echo "-reference:$(cygpath -w "$OUT/Rxdk.Input.dll")"
  echo "$(cygpath -w "$SRC")"
  echo "$(cygpath -w "$ROOT/tests/managed/CompressionExtras.cs")"
  echo "$(cygpath -w "$ROOT/vendor/mono/mcs/class/System/System.IO.Compression/DeflateStream.cs")"
} > "$RSP"

echo "== compiling Main.dll with Roslyn =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$OUT/testasm.log" 2>&1
rc=$?
echo "csc exit $rc"
[ "$rc" = "0" ] && ls -la "$OUT/Main.dll" | awk '{print "Main.dll:", $5, "bytes"}' || { echo "-- errors --"; grep -iE 'error' "$OUT/testasm.log" | head; }
exit $rc
