#!/usr/bin/env bash
# Rxdk.Graphics.dll — managed device over libd3d8. The sidecar names libd3d8 so
# build-host relinks the XBE. A title without this assembly does not link it.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
OUT="$ROOT/build-out/corlib"
CORLIB="$OUT/mscorlib.dll"
SRC="$ROOT/src/Rxdk.Graphics/Graphics.cs"
MATH="$ROOT/src/Rxdk.Graphics/Math.cs"
XG="$ROOT/src/Rxdk.Graphics/XGraphics.cs"
DLL="$OUT/Rxdk.Graphics.dll"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }
mkdir -p "$OUT"

echo "== compiling Rxdk.Graphics.dll =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" -nostdlib -noconfig -target:library -unsafe -optimize+ \
  -out:"$(cygpath -w "$DLL")" -reference:"$(cygpath -w "$CORLIB")" \
  "$(cygpath -w "$SRC")" "$(cygpath -w "$MATH")" "$(cygpath -w "$XG")" > "$OUT/rxdk-graphics.log" 2>&1
rc=$?
if [ "$rc" != 0 ]; then
  echo "Rxdk.Graphics.dll FAILED"
  grep -iE 'error' "$OUT/rxdk-graphics.log" | head
  exit 1
fi
cp "$ROOT/src/Rxdk.Graphics/Rxdk.Graphics.dll.libs" "$OUT/Rxdk.Graphics.dll.libs"
echo "Rxdk.Graphics.dll: $(stat -c%s "$DLL" 2>/dev/null) bytes"
