#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 milestone 3: compile mono/sgen (GC) + mono/metadata (loader/type system)
# for the Xbox. Same recipe/flags as milestones 1-2 (config.h + win32_supplement.h + compat shims,
# HOST_WIN32 variants). Aggregated error reporting; archives libmonoruntime.lib from what compiles.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="/c/ProgramData/RXDK/llvm/xboxog-windows-x64"
CLANG="$TC/bin/clang.exe"
GEN="$ROOT/build/generated/mono"
MONO="$ROOT/vendor/mono"
EGLIB="$MONO/mono/eglib"
SDKI="/c/ProgramData/RXDK/sdk/include"
PAL="$ROOT/pal/include"
OUT="$ROOT/build-out/obj/metadata"
mkdir -p "$OUT"

FLAGS=(
  -target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls
  -fms-extensions
  -DHAVE_CONFIG_H -DHAVE_SGEN_GC=1
  -include "$GEN/config.h"
  -include "$PAL/rxdk/win32_supplement.h"
  -include "$PAL/rxdk/win_crt_compat.h"
  -I "$GEN" -I "$MONO" -I "$MONO/mono" -I "$EGLIB" -I "$SDKI" -I "$PAL" -I "$ROOT/build/generated/compat"
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
)

# Non-HOST_WIN32 platform variants + disabled subsystems (sockets, processes).
EXCLUDE='console-unix|file-mmap-posix|w32error-unix|w32event-unix|w32file-unix|w32mutex-unix|w32semaphore-unix'
EXCLUDE="$EXCLUDE|w32socket-unix|w32socket-win32|w32socket|w32process-unix-bsd|w32process-unix-default|w32process-unix-haiku|w32process-unix-osx|w32process-unix|w32process-win32|w32process"
# disabled subsystems: COM (coree/cominterop/marshal-windows), socket threadpool-io, security, null console
EXCLUDE="$EXCLUDE|coree|cominterop|marshal-windows|threadpool-io|threadpool-io-poll|mono-security-windows|console-null"
# w32file-win32 IS compiled now: the SDK hardcodes WIN32_FIND_DATA->ANSI and ships no
# WIN32_FIND_DATAW, so win32_supplement.h defines the wide struct and w32file.h/.c point their
# find-data at WIN32_FIND_DATAW (xbox branch). The wide Win32 file APIs it calls (FindFirstFileW,
# CreateDirectoryW, ...) are W->A-thunked in pal/src/win32_file_shims.c (the SDK's W file APIs are
# broken on D:\; only the ANSI ones work).

pass=0; fail=0; failed=()
for f in "$MONO"/mono/sgen/*.c "$MONO"/mono/metadata/*.c; do
  b=$(basename "$f" .c)
  echo "$b" | grep -qE "^($EXCLUDE)$" && continue
  if "$CLANG" "${FLAGS[@]}" "$f" -o "$OUT/$b.o" 2> "$OUT/$b.err"; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); failed+=("$b")
  fi
done

echo "===== sgen+metadata: $pass compiled, $fail failed ====="
echo "-- distinct MISSING HEADERS --"
for s in "${failed[@]}"; do cat "$OUT/$s.err"; done | grep -oE "fatal error: '[^']+' file not found" | sort | uniq -c | sort -rn
echo "-- distinct FIRST non-header errors (top 30) --"
for s in "${failed[@]}"; do grep -m1 'error:' "$OUT/$s.err" | grep -v 'file not found'; done | sed -E "s/^[^:]+:[0-9]+:[0-9]+: //" | sort | uniq -c | sort -rn | head -30
echo "-- failed files ($fail) --"; echo "${failed[*]}"

if [ "$pass" -gt 0 ]; then
  LIB="$ROOT/build-out/lib"; mkdir -p "$LIB"
  OUTW=$(cygpath -w "$LIB/libmonoruntime.lib")
  objsw=(); for o in "$OUT"/*.o; do objsw+=("$(cygpath -w "$o")"); done
  MSYS2_ARG_CONV_EXCL='*' "$TC/bin/llvm-lib.exe" "/OUT:$OUTW" "${objsw[@]}" >/dev/null && \
    echo "archived $pass objs -> build-out/lib/libmonoruntime.lib"
fi
