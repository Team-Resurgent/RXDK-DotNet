#!/usr/bin/env bash
# Rxdk.Input.dll — managed GamePad and Keyboard over libxapi. No extra native SDK lib.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"
CORLIB="$OUT/mscorlib.dll"
SRC="$ROOT/src/Rxdk.Input/Input.cs"
DLL="$OUT/Rxdk.Input.dll"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
mkdir -p "$OUT"

echo "== compiling Rxdk.Input.dll =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" -nostdlib -noconfig -target:library -optimize+ \
  -out:"$(cygpath -w "$DLL")" -reference:"$(cygpath -w "$CORLIB")" \
  "$(cygpath -w "$SRC")" > "$OUT/rxdk-input.log" 2>&1
rc=$?
if [ "$rc" != 0 ]; then
  echo "Rxdk.Input.dll FAILED"
  grep -iE 'error' "$OUT/rxdk-input.log" | head
  exit 1
fi
cp "$ROOT/src/Rxdk.Input/Rxdk.Input.dll.libs" "$OUT/Rxdk.Input.dll.libs"
echo "Rxdk.Input.dll: $(stat -c%s "$DLL" 2>/dev/null) bytes"
