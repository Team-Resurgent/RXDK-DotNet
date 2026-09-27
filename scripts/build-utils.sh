#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 milestone 2: compile mono/utils (the OS/PAL abstraction) for the Xbox.
# Compiles every mono/utils/*.c EXCEPT the non-HOST_WIN32 platform variants, with the same
# toolchain/flags as eglib plus the mono include roots. Reports pass/fail and an AGGREGATED view
# of the distinct errors (missing headers, first error per file) so the PAL/symbol gaps are legible.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="/c/ProgramData/RXDK/llvm/xboxog-windows-x64"
CLANG="$TC/bin/clang.exe"
GEN="$ROOT/build/generated/mono"
MONO="$ROOT/vendor/mono"
EGLIB="$MONO/mono/eglib"
SDKI="/c/ProgramData/RXDK/sdk/include"
PAL="$ROOT/pal/include"
OUT="$ROOT/build-out/obj/utils"
mkdir -p "$OUT"

FLAGS=(
  -target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls
  -fms-extensions
  -DHAVE_CONFIG_H
  -include "$GEN/config.h"
  -include "$PAL/rxdk/win32_supplement.h"
  -include "$PAL/rxdk/win_crt_compat.h"
  -I "$GEN" -I "$MONO" -I "$MONO/mono" -I "$EGLIB" -I "$SDKI" -I "$PAL" -I "$ROOT/build/generated/compat"
  -Wno-implicit-function-declaration -Wno-int-conversion
)

# Exclude non-HOST_WIN32 platform variants (keep -windows / -win32).
EXCLUDE='mono-dl-darwin|mono-dl-posix|mono-log-android|mono-log-darwin|mono-log-posix|mono-threads-android|mono-threads-linux|mono-threads-posix|networking-posix|os-event-unix'
# non-x86 arch hwcap + other-platform thread backends (not our target)
EXCLUDE="$EXCLUDE|mono-hwcap-arm|mono-hwcap-arm64|mono-hwcap-riscv|mono-hwcap-s390x|mono-hwcap-sparc|mono-hwcap-ppc|mono-hwcap-wasm|mono-threads-mach|mono-threads-wasm|mono-threads-posix-signals"
# networking (sockets disabled on Xbox bring-up)
EXCLUDE="$EXCLUDE|networking|networking-fallback|networking-missing|networking-windows|mono-networkinterfaces|mono-poll"
# disabled subsystems: processes/psapi, bcrypt-rand, alt allocator, dynamic loading, io-portability
EXCLUDE="$EXCLUDE|mono-proclib|mono-proclib-windows|mono-rand-windows|dlmalloc|mono-dl|mono-dl-windows|mono-embed|mono-io-portability"

pass=0; fail=0; failed=()
for f in "$MONO"/mono/utils/*.c; do
  b=$(basename "$f" .c)
  echo "$b" | grep -qE "^($EXCLUDE)$" && continue
  if "$CLANG" "${FLAGS[@]}" "$f" -o "$OUT/$b.o" 2> "$OUT/$b.err"; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); failed+=("$b")
  fi
done

echo "===== mono/utils: $pass compiled, $fail failed ====="
echo
echo "-- distinct MISSING HEADERS across failures --"
cat "${failed[@]/#/$OUT/}" 2>/dev/null >/dev/null   # noop guard
for s in "${failed[@]}"; do cat "$OUT/$s.err"; done | grep -oE "fatal error: '[^']+' file not found" | sort | uniq -c | sort -rn
echo
echo "-- distinct FIRST non-header errors (top 25) --"
for s in "${failed[@]}"; do grep -m1 'error:' "$OUT/$s.err" | grep -v 'file not found'; done | sed -E "s/^[^:]+:[0-9]+:[0-9]+: //" | sort | uniq -c | sort -rn | head -25
echo
echo "-- failed files ($fail) --"; echo "${failed[*]}"

# Archive whatever compiled into libmonoutils.lib.
if [ "$pass" -gt 0 ]; then
  LIB="$ROOT/build-out/lib"; mkdir -p "$LIB"
  OUTW=$(cygpath -w "$LIB/libmonoutils.lib")
  objsw=(); for o in "$OUT"/*.o; do objsw+=("$(cygpath -w "$o")"); done
  MSYS2_ARG_CONV_EXCL='*' "$TC/bin/llvm-lib.exe" "/OUT:$OUTW" "${objsw[@]}" >/dev/null && \
    echo "archived $pass objs -> build-out/lib/libmonoutils.lib"
fi
