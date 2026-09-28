#!/usr/bin/env bash
# RXDK-DotNet — build the official Mono JIT regression tests (vendor/mono/mono/mini/*.cs) with Roslyn
# against our classic-Mono mscorlib.dll (/nostdlib). Each test file defines its own `class Tests` with
# a Main that calls TestDriver.RunTests(typeof(Tests)); so each is compiled into its OWN assembly
# alongside TestDriver.cs. Output: build-out/corlib/mini-<name>.dll, bundled onto the DVD; the host
# loads each and invokes Tests::Main, which returns the number of failed sub-tests.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSC="/c/Program Files/dotnet/sdk/10.0.400/Roslyn/bincore/csc.dll"
OUT="$ROOT/build-out/corlib"
MINI="$ROOT/vendor/mono/mono/mini"
CORLIB="$OUT/mscorlib.dll"
# Pristine upstream driver. DateTime.Now now works on-device (win32_supplement.c provides the
# kernel32 time-zone P/Invokes via a mono_dl fallback), so --time timing runs unmodified.
DRIVER="$MINI/TestDriver.cs"

[ -f "$CORLIB" ] || { echo "ERROR: $CORLIB not found — build corlib first"; exit 1; }

# Curated set, ordered easiest-first. basic (int arith/control flow), basic-long (int64),
# basic-float (x87), basic-math, arrays, objects (OOP/valuetypes), exceptions, builtin-types,
# devirtualization. generics.cs additionally needs System.Core.dll (Linq) + generics-variant-types.dll.
TESTS="${*:-basic basic-long basic-float basic-math arrays objects exceptions builtin-types devirtualization generics}"

# generics.cs needs System.Core.dll (Linq) + generics-variant-types.dll (variant interfaces the IL
# helper defines; we build the C# equivalent since the RXDK toolchain ships no ilasm).
if echo "$TESTS" | grep -qw generics; then
  [ -f "$OUT/System.Core.dll" ] || bash "$ROOT/scripts/build-syscore.sh" >/dev/null 2>&1
  VARSRC="$ROOT/build/managed/generics-variant-types.cs"
  VARDLL="$OUT/generics-variant-types.dll"
  MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" -nostdlib -noconfig -target:library \
    -out:"$(cygpath -w "$VARDLL")" -reference:"$(cygpath -w "$CORLIB")" "$(cygpath -w "$VARSRC")" \
    > "$OUT/generics-variant-types.log" 2>&1 && echo "  OK   generics-variant-types.dll" \
    || { echo "  FAIL generics-variant-types.dll"; grep -iE 'error CS' "$OUT/generics-variant-types.log" | head; }
fi

ok=0; bad=0; failed=()
for t in $TESTS; do
  SRC="$MINI/$t.cs"
  [ -f "$SRC" ] || { echo "SKIP $t (no $t.cs)"; continue; }
  RSP="$OUT/mini-$t.rsp"
  {
    echo "-nostdlib"; echo "-noconfig"; echo "-target:exe"; echo "-optimize+"
    echo "-unsafe"
    echo "-out:$(cygpath -w "$OUT/mini-$t.dll")"
    echo "-reference:$(cygpath -w "$CORLIB")"
    if [ "$t" = generics ]; then
      echo "-reference:$(cygpath -w "$OUT/System.Core.dll")"
      echo "-reference:$(cygpath -w "$OUT/generics-variant-types.dll")"
    fi
    # builtin-types selects nint/nuint/nfloat storage size by ARCH_<bits>; upstream Makefile passes
    # -define:ARCH_$((8*SIZEOF_VOID_P)). We target i686 (4-byte pointers) -> ARCH_32 (nfloat = Single).
    [ "$t" = builtin-types ] && echo "-define:ARCH_32"
    echo "$(cygpath -w "$DRIVER")"
    echo "$(cygpath -w "$SRC")"
  } > "$RSP"
  MSYS2_ARG_CONV_EXCL='*' dotnet exec "$(cygpath -w "$CSC")" "@$(cygpath -w "$RSP")" > "$OUT/mini-$t.log" 2>&1
  if [ $? = 0 ]; then
    sz=$(stat -c%s "$OUT/mini-$t.dll" 2>/dev/null)
    echo "  OK   mini-$t.dll ($sz bytes)"; ok=$((ok+1))
  else
    echo "  FAIL mini-$t.dll"; bad=$((bad+1)); failed+=("$t")
    grep -iE 'error CS' "$OUT/mini-$t.log" | head -6 | sed 's/^/       /'
  fi
done
echo "===== mini tests: $ok built, $bad failed ====="
[ "$bad" = 0 ] || { echo "failed: ${failed[*]}"; }
exit 0
