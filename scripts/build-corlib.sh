#!/usr/bin/env bash
# RXDK-DotNet — Phase-1b spike: build classic-Mono-6.13 mscorlib.dll with Roslyn (/nostdlib).
# Assembles the net_4_x + win32 source list (minus excludes) and compiles. Reports error magnitude
# and the dominant CS#### categories. See docs/phase1b-corlib.md.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORLIB="$ROOT/vendor/mono/mcs/class/corlib"
CSC="/c/Program Files/dotnet/sdk/10.0.400/Roslyn/bincore/csc.dll"
OUT="$ROOT/build-out/corlib"; mkdir -p "$OUT"

# corlib pulls its core System.* types from mono's external/corefx submodule — ensure it's present
# (dropped errors 41422 -> 1254 in the Phase-1b spike).
if [ ! -f "$CORLIB/../../../external/corefx/src/Common/src/CoreLib/System/Action.cs" ]; then
  echo "== init external/corefx submodule =="
  git -C "$ROOT/vendor/mono" submodule update --init --depth 1 external/corefx external/referencesource 2>&1 | tail -2
fi

grep -hvE '^\s*#|^\s*$' "$CORLIB/corlib.dll.sources" "$CORLIB/win32_net_4_x_corlib.dll.sources" | sort -u > "$OUT/all.txt"
grep -hvE '^\s*#|^\s*$' "$CORLIB/win32_net_4_x_corlib.dll.exclude.sources" 2>/dev/null | sort -u > "$OUT/excl.txt"
comm -23 "$OUT/all.txt" "$OUT/excl.txt" > "$OUT/srcs.txt"

RSP="$OUT/corlib.rsp"
{
  echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-unsafe"; echo "-nowarn:0169,0649,0067,0219,0414,3021,1685"
  echo "-define:NET_4_0;NET_4_5;NET_4_6;MONO"
  echo "-out:$(cygpath -w "$OUT/mscorlib.dll")"
} > "$RSP"
missing=0
while read -r s; do
  p="$CORLIB/$s"
  if [ -f "$p" ]; then cygpath -w "$p" >> "$RSP"; else missing=$((missing+1)); fi
done < "$OUT/srcs.txt"
echo "sources: $(grep -c '\.cs$' "$RSP" | tr -d ' ') files ($missing listed-but-missing)"

echo "== compiling mscorlib.dll with Roslyn =="
MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$OUT/build.log" 2>&1
rc=$?
echo "csc exit $rc"
echo "errors: $(grep -cE ': error CS' "$OUT/build.log")"
echo "-- top error categories --"
grep -oE 'error CS[0-9]+' "$OUT/build.log" | sort | uniq -c | sort -rn | head -15
echo "-- sample errors --"
grep -E ': error CS' "$OUT/build.log" | head -8
[ -f "$OUT/mscorlib.dll" ] && echo "PRODUCED mscorlib.dll ($(stat -c%s "$OUT/mscorlib.dll") bytes)"
