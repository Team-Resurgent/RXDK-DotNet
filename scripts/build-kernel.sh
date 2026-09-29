#!/usr/bin/env bash
# Rxdk.Kernel.dll — kernel exports managed code does not already cover. No extra SDK lib.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"
CORLIB="$OUT/mscorlib.dll"
SRC="$ROOT/src/Rxdk.Kernel/Kernel.cs"
DLL="$OUT/Rxdk.Kernel.dll"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
mkdir -p "$OUT"

echo "== compiling Rxdk.Kernel.dll =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" -nostdlib -noconfig -target:library -unsafe -optimize+ \
  -out:"$(cygpath -w "$DLL")" -reference:"$(cygpath -w "$CORLIB")" \
  "$(cygpath -w "$SRC")" > "$OUT/rxdk-kernel.log" 2>&1
rc=$?
if [ "$rc" != 0 ]; then
  echo "Rxdk.Kernel.dll FAILED"
  grep -iE 'error' "$OUT/rxdk-kernel.log" | head
  exit 1
fi
cp "$ROOT/src/Rxdk.Kernel/Rxdk.Kernel.dll.libs" "$OUT/Rxdk.Kernel.dll.libs"
echo "Rxdk.Kernel.dll: $(stat -c%s "$DLL" 2>/dev/null) bytes"
