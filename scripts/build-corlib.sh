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
if [ ! -f "$CORLIB/../../../external/corert/src/System.Private.CoreLib/shared/System/Threading/CancellationToken.cs" ]; then
  echo "== init external corefx/corert/referencesource submodules =="
  git -C "$ROOT/vendor/mono" submodule update --init --depth 1 external/corefx external/corert external/referencesource 2>&1 | tail -3
fi

# Consts.cs is generated from Consts.cs.in (MonoCorlibVersion must match the runtime).
sed -e 's/@MONO_CORLIB_VERSION@/1A5E0066-58DC-428A-B21C-0AD6CDAE2789/' \
    -e 's/@MONO_VERSION@/6.13.0/' \
    "$CORLIB/../../build/common/Consts.cs.in" > "$CORLIB/../../build/common/Consts.cs"

# win32 net_4_x corlib combines the base + win32_build + win32_net_4_x source lists (and excludes).
grep -hvE '^\s*#|^\s*$' "$CORLIB/corlib.dll.sources" "$CORLIB/win32_build_corlib.dll.sources" "$CORLIB/win32_net_4_x_corlib.dll.sources" | sort -u > "$OUT/all.txt"
grep -hvE '^\s*#|^\s*$' "$CORLIB/win32_build_corlib.dll.exclude.sources" "$CORLIB/win32_net_4_x_corlib.dll.exclude.sources" 2>/dev/null | sort -u > "$OUT/excl.txt"
RSP="$OUT/corlib.rsp"
{
  echo "-nostdlib"; echo "-noconfig"; echo "-target:library"; echo "-unsafe"
  echo "-runtimemetadataversion:v4.0.30319"
  # Delay-sign with the ECMA public key (correct strong-name token: b77a5c561934e089) — overrides
  # AssemblyInfo's relative '../ecma.pub'.
  echo "-delaysign+"
  echo "-keyfile:$(cygpath -w "$CORLIB/../ecma.pub")"
  echo "-nowarn:0169,0649,0067,0219,0414,3021,1685,0612,0618,3001,3002,3003,0809,0672"
  # Full net_4_x corlib define set (mcs/build/profiles/net_4_x.make + class/corlib/Makefile).
  # BIT64 omitted (i686 target); Apple TLS defines omitted.
  echo "-define:NET_4_0;NET_4_5;NET_4_6;MONO;WIN_PLATFORM;INSIDE_CORLIB;MONO_CULTURE_DATA;LIBC;REGISTRY_ASSEMBLY;FEATURE_PAL;GENERICS_WORK;FEATURE_LIST_PREDICATES;FEATURE_SERIALIZATION;FEATURE_ENCODINGNLS;FEATURE_ASCII;FEATURE_LATIN1;FEATURE_UTF7;FEATURE_UTF32;MONO_HYBRID_ENCODING_SUPPORT;FEATURE_ASYNC_IO;NEW_EXPERIMENTAL_ASYNC_IO;FEATURE_EXCEPTIONDISPATCHINFO;FEATURE_CORRUPTING_EXCEPTIONS;FEATURE_EXCEPTION_NOTIFICATIONS;FEATURE_STRONGNAME_MIGRATION;FEATURE_USE_LCID;FEATURE_FUSION;FEATURE_CRYPTO;FEATURE_X509_SECURESTRINGS;FEATURE_SYNCHRONIZATIONCONTEXT;FEATURE_SYNCHRONIZATIONCONTEXT_WAIT;FEATURE_DEFAULT_INTERFACES;HAS_CORLIB_CONTRACTS;FEATURE_MACL;FEATURE_REMOTING;MONO_COM;FEATURE_COMINTEROP;FEATURE_ROLE_BASED_SECURITY;MONO_FEATURE_THREAD_ABORT;MONO_FEATURE_THREAD_SUSPEND_RESUME;MONO_FEATURE_MULTIPLE_APPDOMAINS;MONO_FEATURE_SRE;MONO_FEATURE_CONSOLE"
  echo "-out:$(cygpath -w "$OUT/mscorlib.dll")"
} > "$RSP"
# mono .sources entries may be globs with a ':excluded,files' suffix (e.g. "dir/*.cs:Foo.cs").
# Expand BOTH includes and excludes to resolved paths, then subtract — so glob-re-included files
# that a *.exclude.sources means to drop (e.g. RegistryAccessRule) are actually removed.
expand() {  # $1 = .sources list -> resolved windows paths on stdout
  while read -r s; do
    s="${s%$'\r'}"
    base="${s%%:*}"; ex=""; [ "$s" != "$base" ] && ex=",${s#*:},"
    case "$base" in
      *\**) for f in $CORLIB/$base; do [ -f "$f" ] || continue; bn=$(basename "$f")
              [ -n "$ex" ] && printf '%s' "$ex" | grep -q ",$bn," && continue
              cygpath -w "$f"; done ;;
      *)    [ -f "$CORLIB/$base" ] && cygpath -w "$CORLIB/$base" ;;
    esac
  done < "$1"
}
expand "$OUT/all.txt"  | sort -u > "$OUT/paths.txt"
expand "$OUT/excl.txt" | sort -u > "$OUT/exclpaths.txt"
comm -23 "$OUT/paths.txt" "$OUT/exclpaths.txt" >> "$RSP"
echo "sources: $(grep -c '\.cs"\?$' "$RSP" | tr -d ' ') files"

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
