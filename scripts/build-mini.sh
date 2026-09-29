#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 milestone 4: compile mono/mini + mono/mini/interp (JIT-driver, interpreter,
# x86 arch layer) for the Xbox. Same recipe as milestones 1-3. Keeps x86 + common + windows + interp;
# excludes every other arch, LLVM, the AOT compiler, and mini-windows-dllmain (DLL semantics we
# replace with explicit init). Aggregated errors; archives libmini.lib.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
GEN="$ROOT/build/generated/mono"
MONO="$ROOT/vendor/mono"
EGLIB="$MONO/mono/eglib"
PAL="$ROOT/pal/include"
OUT="$ROOT/build-out/obj/mini"
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
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types
)

# Keep x86 + common + windows + interp. Exclude other arches, LLVM, AOT-compiler, other platforms.
EXCLUDE='(mini|tramp|exceptions)-(amd64|arm|arm64|mips|ppc|riscv|s390x|sparc|wasm|loongarch64)(-gsharedvt)?'
# Keep aot-runtime + llvmonly-runtime IN: under DISABLE_AOT / !ENABLE_LLVM they provide the no-op
# mono_aot_* / mini_llvmonly_* definitions the interp path references. Only the AOT *compiler* and
# the real LLVM backend stay out.
EXCLUDE="$EXCLUDE|mini-llvm|aot-compiler|aot-runtime-wasm|mini-posix|mini-darwin|mini-wasm-debugger|mini-windows-dllmain|whitebox"
# embedded runtime: no mono.exe launcher (we write our own host), no socket soft-debugger, no
# DAC/TLS-callback bootstrap (replaced by explicit init).
EXCLUDE="$EXCLUDE|main|main-sgen|debugger-agent|mini-windows-dlldac|mini-windows-tls-callback"

pass=0; fail=0; failed=()
for f in "$MONO"/mono/mini/*.c "$MONO"/mono/mini/interp/*.c; do
  b=$(basename "$f" .c)
  echo "$b" | grep -qE "^($EXCLUDE)$" && continue
  if "$CLANG" "${FLAGS[@]}" "$f" -o "$OUT/$b.o" 2> "$OUT/$b.err"; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); failed+=("$b")
  fi
done

echo "===== mini+interp: $pass compiled, $fail failed ====="
echo "-- distinct MISSING HEADERS --"
for s in "${failed[@]}"; do cat "$OUT/$s.err"; done | grep -oE "fatal error: '[^']+' file not found" | sort | uniq -c | sort -rn
echo "-- distinct FIRST non-header errors (top 30) --"
for s in "${failed[@]}"; do grep -m1 'error:' "$OUT/$s.err" | grep -v 'file not found'; done | sed -E "s/^[^:]+:[0-9]+:[0-9]+: //" | sort | uniq -c | sort -rn | head -30
echo "-- failed files ($fail) --"; echo "${failed[*]}"
# A source that stops compiling is a broken archive, not a partial one. Fail here so a
# release cannot be cut from an incomplete libmini.lib.
if [ "$fail" -gt 0 ]; then exit 1; fi

if [ "$pass" -gt 0 ]; then
  LIB="$ROOT/build-out/lib"; mkdir -p "$LIB"
  OUTW=$(cygpath -w "$LIB/libmini.lib")
  objsw=(); for o in "$OUT"/*.o; do objsw+=("$(cygpath -w "$o")"); done
  MSYS2_ARG_CONV_EXCL='*' "$TC/bin/llvm-lib.exe" "/OUT:$OUTW" "${objsw[@]}" >/dev/null && \
    echo "archived $pass objs -> build-out/lib/libmini.lib"
fi
