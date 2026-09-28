#!/usr/bin/env bash
# musl-link.sh — OUR linker against a real, GCC-built libc: musl-link.c is compiled by GCC against musl, linked by
# toolchain/ld with musl's crt1/crti/libc.a (1400+ members, pulled through the archive index) + a compiler
# runtime (RT=ours: toolchain/rt's libosrt.a; RT=gcc: GCC's libgcc.a) + crtn, then booted as /init on the
# reference kernel. PASS = the program's line reaches the console.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); T=$(cd "$HERE/.." && pwd)
: "${MUSL:?MUSL=<musl static stage (usr/lib/libc.a + crt*.o)>}" "${GNU:?GNU=<cross-gcc prefix>}"
: "${REFKERNEL:?}" "${GEN_INIT_CPIO:?}"; RT=${RT:-ours}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
case "$RT" in ours) RTLIB="$T/rt/build/libosrt.a" ;; gcc) RTLIB=$("${GNU}gcc" -print-libgcc-file-name) ;; *) echo "RT=ours|gcc"; exit 2 ;; esac
L="$MUSL/usr/lib"
"${GNU}gcc" -O2 --sysroot="$MUSL" -c -o "$W/t.o" "$HERE/musl-link.c"
"$T/ld/build/ld" -o "$W/init" "$L/crt1.o" "$L/crti.o" "$W/t.o" "$L/libc.a" "$RTLIB" "$L/crtn.o"
{ echo 'dir /dev 0755 0 0'; echo 'nod /dev/console 0600 0 0 c 5 1'; echo "file /init $W/init 0755 0 0"; } > "$W/list"
"$GEN_INIT_CPIO" "$W/list" | gzip -9 > "$W/initrd"
timeout 60 qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none -kernel "$REFKERNEL" \
  -initrd "$W/initrd" -append "console=ttyAMA0 rdinit=/init panic=1" > "$W/log" 2>&1 || true
want="musl-link 13579 142857142874 7.500 erange"
if grep -qF "$want" "$W/log"; then echo "PASS musl link (RT=$RT): our ld + musl libc.a booted: '$want'"; exit 0; fi
echo "FAIL musl link (RT=$RT) — last lines:"; tail -8 "$W/log" | sed 's/^/    /'; exit 1
