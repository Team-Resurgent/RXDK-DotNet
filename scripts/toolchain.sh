# Resolve the RXDK clang, SDK, host tools, and Roslyn csc.
# Sourced by scripts/build-*.sh after ROOT is set.
#
# A machine that already has the legacy xboxog package keeps using it. CI only
# downloads the unified rolling clang (xbox-windows-x64), whose resource dir is
# not pinned to clang 23.
: "${RXDK_ROOT:=/c/ProgramData/RXDK}"

if [ -x "$RXDK_ROOT/llvm/xboxog-windows-x64/bin/clang.exe" ]; then
  TC="$RXDK_ROOT/llvm/xboxog-windows-x64"
elif [ -x "$RXDK_ROOT/llvm/xbox-windows-x64/bin/clang.exe" ]; then
  TC="$RXDK_ROOT/llvm/xbox-windows-x64"
else
  echo "ERROR: no RXDK clang under $RXDK_ROOT/llvm (expected xboxog-windows-x64 or xbox-windows-x64)" >&2
  exit 1
fi
CLANG="$TC/bin/clang.exe"
LLVMLIB="$TC/bin/llvm-lib.exe"
SDKI="$RXDK_ROOT/sdk/include"
SDKL="$RXDK_ROOT/sdk/lib"
RXDK_TOOLS="$RXDK_ROOT/tools"

BUILTINS=""
for f in "$TC"/lib/clang/*/lib/windows/libclang_rt.builtins-i386.a; do
  if [ -f "$f" ]; then BUILTINS="$f"; break; fi
done
if [ -z "$BUILTINS" ]; then
  echo "ERROR: libclang_rt.builtins-i386.a not found under $TC/lib/clang" >&2
  exit 1
fi

if [ -z "${CSC:-}" ]; then
  if [ -f "/c/Program Files/dotnet/sdk/10.0.400/Roslyn/bincore/csc.dll" ]; then
    CSC="/c/Program Files/dotnet/sdk/10.0.400/Roslyn/bincore/csc.dll"
  else
    CSC="$(ls -d "/c/Program Files/dotnet/sdk/"*/Roslyn/bincore/csc.dll 2>/dev/null | sort -V | tail -n 1)"
  fi
fi
if [ -z "${CSC:-}" ] || [ ! -f "$CSC" ]; then
  echo "ERROR: Roslyn csc.dll not found under /c/Program Files/dotnet/sdk" >&2
  exit 1
fi
