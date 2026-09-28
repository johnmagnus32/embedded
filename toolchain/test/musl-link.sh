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
# boot $W/init as PID 1; it must print its line AND exit with status 0 (the kernel reports init's exit code)
boot() {
  { echo 'dir /dev 0755 0 0'; echo 'nod /dev/console 0600 0 0 c 5 1'; echo "file /init $W/init 0755 0 0"; } > "$W/list"
  "$GEN_INIT_CPIO" "$W/list" | gzip -9 > "$W/initrd"
  timeout 60 qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none -kernel "$REFKERNEL" \
    -initrd "$W/initrd" -append "console=ttyAMA0 rdinit=/init panic=1" > "$W/log" 2>&1 || true
}
check() {   # check NAME WANT...: every WANT string in the log, and a clean exit
  local name=$1; shift
  for w in "$@"; do grep -qF -- "$w" "$W/log" || { echo "FAIL $name: '$w' missing — last lines:"; tail -8 "$W/log" | sed 's/^/    /'; exit 1; }; done
  grep -qF 'exitcode=0x00000000' "$W/log" || { echo "FAIL $name: init did not exit 0 — last lines:"; tail -8 "$W/log" | sed 's/^/    /'; exit 1; }
}
"${GNU}gcc" -O2 --sysroot="$MUSL" -c -o "$W/t.o" "$HERE/musl-link.c"
"$T/ld/build/ld" -o "$W/init" "$L/crt1.o" "$L/crti.o" "$W/t.o" "$L/libc.a" "$RTLIB" "$L/crtn.o"
boot
want="musl-link 13579 142857142874 7.500 erange"
check "musl link (RT=$RT)" "$want" "musl-link flushed-at-exit"
echo "PASS musl link (RT=$RT): our ld + musl libc.a booted, exit flushed stdio, status 0: '$want'"

# thread-local storage: a second program, two TUs (non-PIC: local/initial-exec; -fPIC: general/local-dynamic)
"${GNU}gcc" -O2 --sysroot="$MUSL" -c -o "$W/tls.o" "$HERE/musl-tls.c"
"${GNU}gcc" -O2 --sysroot="$MUSL" -fPIC -c -o "$W/tls2.o" "$HERE/musl-tls2.c"
"$T/ld/build/ld" -o "$W/init" "$L/crt1.o" "$L/crti.o" "$W/tls.o" "$W/tls2.o" "$L/libc.a" "$RTLIB" "$L/crtn.o"
boot
want="musl-tls 1 m 9 154 70 50"
check "musl TLS (RT=$RT, GCC-compiled)" "$want"
echo "PASS musl TLS (RT=$RT): GCC-compiled, per-thread .tdata/.tbss through LE/IE/GD/LD, status 0"

# ...and the same TLS program compiled by OUR cpp/cc/as (initial/local-exec; general-dynamic in the -fPIC half)
for f in musl-tls-cc musl-tls-cc2; do
  pic=; [ "$f" = musl-tls-cc2 ] && pic=-fPIC
  "$T/cpp/build/cpp" "$HERE/$f.c" > "$W/$f.i" && "$T/cc/build/cc" $pic -o "$W/$f.s" "$W/$f.i" && "$T/as/build/as" -o "$W/$f.o" "$W/$f.s"
done
"$T/ld/build/ld" -o "$W/init" "$L/crt1.o" "$L/crti.o" "$W/musl-tls-cc.o" "$W/musl-tls-cc2.o" "$L/libc.a" "$RTLIB" "$L/crtn.o"
boot
check "musl TLS (RT=$RT, our cc)" "$want"
echo "PASS musl TLS (RT=$RT): our cc's _Thread_local, per-thread, status 0: '$want'"
