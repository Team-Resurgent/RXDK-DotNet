#!/usr/bin/env bash
# RXDK-DotNet — Phase-1 endgame: compile the PAL glue + embedding host and trial-link against the
# four mono archives + RXDK-SDK, downgrading unresolved symbols to warnings so lld lists the full
# remaining undefined surface (the true PAL/runtime gap). Aggregates the distinct missing symbols.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/toolchain.sh"
GEN="$ROOT/build/generated/mono"
MONO="$ROOT/vendor/mono"
EGLIB="$MONO/mono/eglib"
PAL="$ROOT/pal/include"
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
"$CLANG" "${CFLAGS[@]}" "$ROOT/pal/src/mono_stubs.c"       -o "$OUT/mono_stubs.o"       || exit 1
"$CLANG" "${CFLAGS[@]}" "$ROOT/pal/src/win_cdecl_shims.c"  -o "$OUT/win_cdecl_shims.o"  || exit 1
"$CLANG" "${CFLAGS[@]}" "$ROOT/pal/src/win32_file_shims.c" -o "$OUT/win32_file_shims.o" || exit 1
# Mono's culture tables (vendor/mono/mono/culture/locales.c). Not part of the metadata archive;
# linked here so CultureInfo/RegionInfo/GetCultures resolve. The matching mono_stubs.c entries
# must stay commented out or they shadow these symbols.
"$CLANG" "${CFLAGS[@]}" -DHAVE_SGEN_GC=1 -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types \
  "$ROOT/vendor/mono/mono/culture/locales.c" -o "$OUT/locales.o" || exit 1
# Bundled zlib (vendor/mono/mono/zlib) + the DeflateStream helper. zutil.c is -DZ_SOLO because
# gzguts.h includes io.h, which the SDK does not ship; zcalloc/zcfree live in win32_supplement.c.
ZLIB="$ROOT/vendor/mono/mono/zlib"
ZFLAGS=(-target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls -fms-extensions
  -I "$ZLIB" -I "$SDKI" -w)
for zf in adler32 crc32 deflate inflate inftrees inffast trees; do
  "$CLANG" "${ZFLAGS[@]}" "$ZLIB/$zf.c" -o "$OUT/z_$zf.o" || exit 1
done
"$CLANG" "${ZFLAGS[@]}" -DZ_SOLO "$ZLIB/zutil.c" -o "$OUT/z_zutil.o" || exit 1
"$CLANG" "${CFLAGS[@]}" -Wno-implicit-function-declaration \
  "$ROOT/vendor/mono/support/zlib-helper.c" -o "$OUT/zlib-helper.o" || exit 1
# Sockets: Mono's w32socket over Xbox XNet (libxnet). winsock2.h/ws2tcpip.h in pal/include/rxdk
# stand in for the headers the SDK does not ship. TransmitFile/DisconnectEx are off (no mswsock.h).
# The matching mono_stubs.c entries must stay commented out.
SFLAGS=(
  -target i686-pc-windows-gnu -march=pentium3 -c -O1 -g0
  -ffreestanding -fno-stack-protector -fno-sanitize=undefined -femulated-tls -fms-extensions
  -DHAVE_CONFIG_H -DHAVE_SGEN_GC=1
  -DHAVE_API_SUPPORT_WIN32_TRANSMIT_FILE=0 -DHAVE_API_SUPPORT_WIN32_DISCONNECT_EX=0
  -DRXDK_WSABUF_DEFINED -DRXDK_WSAEVENT_DEFINED
  -include "$GEN/config.h" -include "$PAL/rxdk/win32_supplement.h" -include "$PAL/rxdk/win_crt_compat.h"
  -I "$GEN" -I "$MONO" -I "$MONO/mono" -I "$EGLIB"
  -I "$PAL/rxdk" -I "$SDKI" -I "$PAL" -I "$ROOT/build/generated/compat"
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -w
)
"$CLANG" "${SFLAGS[@]}" "$ROOT/vendor/mono/mono/metadata/w32socket.c" -o "$OUT/w32socket.o" || exit 1
"$CLANG" "${SFLAGS[@]}" "$ROOT/vendor/mono/mono/metadata/w32socket-win32.c" -o "$OUT/w32socket-win32.o" || exit 1
"$CLANG" "${SFLAGS[@]}" "$ROOT/vendor/mono/mono/utils/networking.c" -o "$OUT/networking.o" || exit 1
"$CLANG" "${SFLAGS[@]}" -DRXDK_XBOX_SELECT "$ROOT/vendor/mono/mono/utils/mono-poll.c" -o "$OUT/mono-poll.o" || exit 1
"$CLANG" "${SFLAGS[@]}" "$ROOT/pal/src/xbox_net.c" -o "$OUT/xbox_net.o" || exit 1
# Real threadpool worker, linked ahead of the metadata archive so it beats any
# threadpool-worker-wasm.o still sitting in an older libmonoruntime.lib.
"$CLANG" "${CFLAGS[@]}" -DHAVE_SGEN_GC=1 -include "$PAL/rxdk/win32_supplement.h" \
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types \
  "$MONO/mono/metadata/threadpool-worker-default.c" -o "$OUT/threadpool-worker-default.o" || exit 1
# Ahead of the metadata archive so this sgen-mono.c beats the copy inside libmonoruntime.lib.
"$CLANG" "${CFLAGS[@]}" -DHAVE_SGEN_GC=1 -include "$PAL/rxdk/win32_supplement.h" \
  -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types \
  "$MONO/mono/metadata/sgen-mono.c" -o "$OUT/sgen-mono.o" || exit 1
"$CLANG" "${CFLAGS[@]}" "$ROOT/tests/mono-host/host_main.c" -o "$OUT/host_main.o"        || exit 1
echo "   ok"

echo "== trial link (unresolved -> warnings, to enumerate the gap) =="
W() { cygpath -w "$1"; }
MSYS2_ARG_CONV_EXCL='*' "$CLANG" \
  "$(W "$OUT/host_main.o")" "$(W "$OUT/win32_supplement.o")" "$(W "$OUT/win_crt_compat.o")" "$(W "$OUT/win_cdecl_shims.o")" "$(W "$OUT/win32_file_shims.o")" "$(W "$OUT/locales.o")" \
  "$(W "$OUT/z_adler32.o")" "$(W "$OUT/z_crc32.o")" "$(W "$OUT/z_deflate.o")" "$(W "$OUT/z_inflate.o")" "$(W "$OUT/z_inftrees.o")" "$(W "$OUT/z_inffast.o")" "$(W "$OUT/z_trees.o")" "$(W "$OUT/z_zutil.o")" "$(W "$OUT/zlib-helper.o")" \
  "$(W "$OUT/w32socket.o")" "$(W "$OUT/w32socket-win32.o")" "$(W "$OUT/networking.o")" "$(W "$OUT/mono-poll.o")" "$(W "$OUT/xbox_net.o")" \
  "$(W "$OUT/threadpool-worker-default.o")" "$(W "$OUT/sgen-mono.o")" \
  -Wl,--start-group \
  "$(W "$LIB/libmini.lib")" "$(W "$LIB/libmonoruntime.lib")" "$(W "$LIB/libmonoutils.lib")" "$(W "$LIB/libeglib.lib")" \
  "$(W "$SDKL/libxnet.lib")" "$(W "$SDKL/libxapi.lib")" "$(W "$SDKL/libkernel.lib")" "$(W "$SDKL/libc.lib")" "$(W "$SDKL/libcpp.lib")" "$(W "$SDKL/libcompat.lib")" \
  -Wl,--end-group \
  "$(W "$BUILTINS")" "$(W "$OUT/mono_stubs.o")" \
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

# ---- package the linked PE into a bootable XBE + ISO (RXDK tools) --------------------------------
if [ -f "$OUT/mono-host.exe" ]; then
  T="$RXDK_TOOLS"
  MSYS2_ARG_CONV_EXCL='*' "$T/imagebld.exe" "/in:$(W "$OUT/mono-host.exe")" "/out:$(W "$OUT/mono-host.xbe")" \
    /nologo /stack:1048576 /debug /nolibwarn /dontmountud /TESTID:0xffff0002 /TESTNAME:RxdkMonoHost /TESTVERSION:4096 >/dev/null 2>&1
  rm -rf "$OUT/iso"; mkdir -p "$OUT/iso/RxdkMonoHost/assy"
  cp "$OUT/mono-host.xbe" "$OUT/iso/RxdkMonoHost/default.xbe"
  # bundle the managed BCL + test assembly in D:\assy (mono_set_assemblies_path). Keep a root copy
  # of mscorlib for the file-access probe.
  if [ -f "$ROOT/build-out/corlib/mscorlib.dll" ]; then
    cp "$ROOT/build-out/corlib/mscorlib.dll" "$OUT/iso/RxdkMonoHost/assy/mscorlib.dll"
    cp "$ROOT/build-out/corlib/mscorlib.dll" "$OUT/iso/RxdkMonoHost/mscorlib.dll"
  fi
  [ -f "$ROOT/build-out/corlib/Test.dll" ] && cp "$ROOT/build-out/corlib/Test.dll" "$OUT/iso/RxdkMonoHost/assy/Test.dll"
  # official Mono JIT regression tests (scripts/build-minitests.sh -> mini-*.dll), each its own assembly
  for d in "$ROOT"/build-out/corlib/mini-*.dll; do [ -f "$d" ] && cp "$d" "$OUT/iso/RxdkMonoHost/assy/"; done
  # extra managed assemblies some mini tests reference (System.Core = Linq; generics-variant-types)
  for d in System System.Core generics-variant-types; do
    [ -f "$ROOT/build-out/corlib/$d.dll" ] && cp "$ROOT/build-out/corlib/$d.dll" "$OUT/iso/RxdkMonoHost/assy/"
  done
  MSYS2_ARG_CONV_EXCL='*' "$T/xdvdfs.exe" pack "$(W "$OUT/iso/RxdkMonoHost")" "$(W "$OUT/RxdkMonoHost.iso")" >/dev/null 2>&1
  echo "packaged -> build-out/obj/host/RxdkMonoHost.iso  (boot: xemu -dvd_path <iso> -device lpc47m157 -serial stdio)"
fi
und=$(wc -l < "$OUT/undef.txt" | tr -d '[:space:]')
if [ "$und" != 0 ] || [ ! -f "$OUT/RxdkMonoHost.iso" ]; then
  echo "ERROR: link undefined=$und or ISO missing"
  exit 1
fi
