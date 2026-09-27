#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 endgame: compile the PAL glue + embedding host and trial-link against the
# four mono archives + RXDK-SDK, downgrading unresolved symbols to warnings so lld lists the full
# remaining undefined surface (the true PAL/runtime gap). Aggregates the distinct missing symbols.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="/c/ProgramData/RXDK/llvm/xboxog-windows-x64"
CLANG="$TC/bin/clang.exe"
GEN="$ROOT/build/generated/mono"
MONO="$ROOT/vendor/mono"
EGLIB="$MONO/mono/eglib"
SDKI="/c/ProgramData/RXDK/sdk/include"
SDKL="/c/ProgramData/RXDK/sdk/lib"
PAL="$ROOT/pal/include"
BUILTINS="$TC/lib/clang/23/lib/windows/libclang_rt.builtins-i386.a"
OUT="$ROOT/build-out/obj/host"; LIB="$ROOT/build-out/lib"
mkdir -p "$OUT"

CFLAGS=(
  -target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls -fms-extensions
  -DHAVE_CONFIG_H -include "$GEN/config.h" -include "$PAL/rxdk/win_crt_compat.h"
  -I "$GEN" -I "$MONO" -I "$MONO/mono" -I "$EGLIB" -I "$SDKI" -I "$PAL" -I "$ROOT/build/generated/compat"
  -w
)

echo "== compile PAL glue + host =="
"$CLANG" "${CFLAGS[@]}" "$ROOT/pal/src/win32_supplement.c" -o "$OUT/win32_supplement.o" || exit 1
"$CLANG" "${CFLAGS[@]}" "$ROOT/pal/src/win_crt_compat.c"   -o "$OUT/win_crt_compat.o"   || exit 1
"$CLANG" "${CFLAGS[@]}" "$ROOT/tests/mono-host/host_main.c" -o "$OUT/host_main.o"        || exit 1
echo "   ok"

echo "== trial link (unresolved -> warnings, to enumerate the gap) =="
W() { cygpath -w "$1"; }
MSYS2_ARG_CONV_EXCL='*' "$CLANG" \
  "$(W "$OUT/host_main.o")" "$(W "$OUT/win32_supplement.o")" "$(W "$OUT/win_crt_compat.o")" \
  -Wl,--start-group \
  "$(W "$LIB/libmini.lib")" "$(W "$LIB/libmonoruntime.lib")" "$(W "$LIB/libmonoutils.lib")" "$(W "$LIB/libeglib.lib")" \
  "$(W "$SDKL/libxapi.lib")" "$(W "$SDKL/libkernel.lib")" "$(W "$SDKL/libc.lib")" "$(W "$SDKL/libcpp.lib")" "$(W "$SDKL/libcompat.lib")" \
  -Wl,--end-group \
  "$(W "$BUILTINS")" \
  -target i686-pc-windows-gnu -march=pentium3 -nostdlib -nostartfiles \
  -Wl,--image-base=0x10000 -fuse-ld=lld -e XapiTitleStartup \
  -Wl,--error-limit=0 -Wl,--allow-multiple-definition \
  -o "$(W "$OUT/mono-host.exe")" 2> "$OUT/link.err"
echo "   link exit $?"

echo "== distinct undefined symbols ($(grep -c 'undefined symbol' "$OUT/link.err")) =="
grep -oE 'undefined symbol: [^ ]+' "$OUT/link.err" | sed 's/undefined symbol: //' | sort -u > "$OUT/undef.txt"
wc -l < "$OUT/undef.txt"
echo "-- sample (first 60) --"; head -60 "$OUT/undef.txt"
echo "-- other link errors (non-undefined) --"; grep -v 'undefined symbol' "$OUT/link.err" | grep -iE 'error|warning' | head -10
