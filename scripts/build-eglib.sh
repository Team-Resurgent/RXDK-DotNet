#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 milestone 1: compile the eglib subset for the Xbox target.
# Compiles the platform-neutral + win32 eglib sources with the RXDK clang (i686-pc-windows-gnu,
# -march=pentium3), our hand-written config.h/eglib-config.h, and the MSVCRT compat shim, then
# archives libeglib.lib with llvm-lib. Reports per-file pass/fail.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
GEN="$ROOT/build/generated/mono"
EGLIB="$ROOT/vendor/mono/mono/eglib"
PAL="$ROOT/pal/include"
OUT="$ROOT/build-out/obj/eglib"
LIB="$ROOT/build-out/lib"
mkdir -p "$OUT" "$LIB"

FLAGS=(
  -target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls
  -fms-extensions
  -DHAVE_CONFIG_H
  -include "$GEN/config.h"
  -include "$PAL/rxdk/win_crt_compat.h"
  -I "$GEN" -I "$EGLIB" -I "$SDKI" -I "$PAL" -I "$ROOT/build/generated/compat"
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
)

# Platform-neutral core + win32 backends. Excluded: *-unix/*-posix/*-aix, gclock-nanosleep,
# gmodule* (dynamic loading — disabled on Xbox).
#
# gmisc-win32 and gunicode-win32 are excluded for good, not deferred: the console has
# no GetLocaleInfoW/GetLocaleInfoEx and no GetACP/GetCPInfoExW. pal/src supplies
# monoeg_g_win32_getlocale and monoeg_g_get_charset instead, and CultureInfo comes
# from pal/src/locales.c.
SRCS=(
  garray gbytearray gerror gfile ghashtable giconv glist gmarkup gmem goutput
  gpath gpattern gptrarray gqsort gqueue gshell gslist gspawn gstr gstring
  gunicode gutf8
  gdate-win32 gdir-win32 gfile-win32 gtimer-win32
)

# gdir-win32.c includes only winsock2.h, so HANDLE and INVALID_HANDLE_VALUE are not in
# scope on this target. The APIs it wants do exist: pal/src/win32_file_shims.c provides
# FindFirstFileW and FindNextFileW over the ANSI originals, and FindClose is an SDK
# macro for CloseHandle.
declare -A EXTRA_INCLUDE=( [gdir-win32]=windows.h )

pass=0; fail=0; failed=()
objs=()
for s in "${SRCS[@]}"; do
  extra=()
  [ -n "${EXTRA_INCLUDE[$s]:-}" ] && extra=(-include "${EXTRA_INCLUDE[$s]}")
  if "$CLANG" "${FLAGS[@]}" "${extra[@]+"${extra[@]}"}" "$EGLIB/$s.c" -o "$OUT/$s.o" 2> "$OUT/$s.err"; then
    pass=$((pass+1)); objs+=("$OUT/$s.o")
  else
    fail=$((fail+1)); failed+=("$s")
  fi
done

echo "eglib: $pass compiled, $fail failed"
if [ "$fail" -gt 0 ]; then
  for s in "${failed[@]}"; do
    echo "----- $s -----"; grep -m3 'error:' "$OUT/$s.err"
  done
  exit 1
fi

if [ "$pass" -gt 0 ]; then
  OUTW=$(cygpath -w "$LIB/libeglib.lib")
  objsw=(); for o in "${objs[@]}"; do objsw+=("$(cygpath -w "$o")"); done
  MSYS2_ARG_CONV_EXCL='*' "$LLVMLIB" "/OUT:$OUTW" "${objsw[@]}" >/dev/null && \
    echo "archived $pass objs -> build-out/lib/libeglib.lib"
fi
