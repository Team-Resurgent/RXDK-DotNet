#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 milestone 1: compile the eglib subset for the Xbox target.
# Compiles the platform-neutral + win32 eglib sources with the RXDK clang (i686-pc-windows-gnu,
# -march=pentium3), our hand-written config.h/eglib-config.h, and the MSVCRT compat shim, then
# archives libeglib.lib with llvm-lib. Reports per-file pass/fail.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="/c/ProgramData/RXDK/llvm/xboxog-windows-x64"
CLANG="$TC/bin/clang.exe"
LLVMLIB="$TC/bin/llvm-lib.exe"
GEN="$ROOT/build/generated/mono"
EGLIB="$ROOT/vendor/mono/mono/eglib"
SDKI="/c/ProgramData/RXDK/sdk/include"
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
  -Wno-implicit-function-declaration
)

# Platform-neutral core + win32 backends. Excluded: *-unix/*-posix/*-aix, gclock-nanosleep,
# gmodule* (dynamic loading — disabled on Xbox).
SRCS=(
  garray gbytearray gerror gfile ghashtable giconv glist gmarkup gmem goutput
  gpath gpattern gptrarray gqsort gqueue gshell gslist gspawn gstr gstring
  gunicode gutf8
  gdate-win32 gdir-win32 gfile-win32 gmisc-win32 gtimer-win32 gunicode-win32
)

pass=0; fail=0; failed=()
objs=()
for s in "${SRCS[@]}"; do
  if "$CLANG" "${FLAGS[@]}" "$EGLIB/$s.c" -o "$OUT/$s.o" 2> "$OUT/$s.err"; then
    pass=$((pass+1)); objs+=("$OUT/$s.o")
  else
    fail=$((fail+1)); failed+=("$s")
  fi
done

echo "eglib: $pass compiled, $fail failed"
if [ "$fail" -gt 0 ]; then
  echo "DEFERRED (win32 backends needing more Win32 header/CRT compat): ${failed[*]}"
  for s in "${failed[@]}"; do
    echo "----- $s (first errors) -----"; grep -m3 'error:' "$OUT/$s.err"
  done
fi

# Archive whatever compiled — the core eglib is the milestone-1 deliverable; the deferred
# win32 backends get folded in once the Win32 header-compat sub-task lands.
if [ "$pass" -gt 0 ]; then
  OUTW=$(cygpath -w "$LIB/libeglib.lib")
  objsw=(); for o in "${objs[@]}"; do objsw+=("$(cygpath -w "$o")"); done
  MSYS2_ARG_CONV_EXCL='*' "$LLVMLIB" "/OUT:$OUTW" "${objsw[@]}" >/dev/null && \
    echo "archived $pass objs -> build-out/lib/libeglib.lib"
fi
