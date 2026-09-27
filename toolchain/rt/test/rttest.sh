#!/usr/bin/env bash
# rttest.sh — build rttest.c three ways and require the same log: GCC + libgcc (the reference), GCC + our
# libosrt (GCC's calls resolve to OUR helpers), our cc + libosrt (our cc's calling convention to them).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); RT=$(cd "$HERE/.." && pwd); T=$(cd "$RT/.." && pwd)
X=${GNU:?GNU = the reference cross-gcc prefix}; QEMU=${QEMU:-qemu-system-arm}
[ -x "${X}gcc" ] || { echo "SKIP: no reference gcc at ${X}gcc"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
GCCF="-O2 -marm -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -ffreestanding -fno-builtin"   # no hardware divide: division is lowered to calls
"${T}/as/build/as" -o "$W/start.o" "$HERE/start.s"
"${X}gcc" $GCCF ${VFLAG:-} -c -o "$W/g.o" "$HERE/rttest.c"
"${T}/cpp/build/cpp" ${VFLAG:-} "$HERE/rttest.c" > "$W/o.i" && "${T}/cc/build/cc" -o "$W/o.s" "$W/o.i" && "${T}/as/build/as" -o "$W/o.o" "$W/o.s"
LIBGCC=$("${X}gcc" $GCCF -print-libgcc-file-name)
"${X}ld" -Ttext=0x40000000 -o "$W/ref.elf" "$W/start.o" "$W/g.o" "$LIBGCC"
"${T}/ld/build/ld" -Ttext 0x40000000 -o "$W/gcc_ours.elf" "$W/start.o" "$W/g.o" "$RT/build/libosrt.a"
"${T}/ld/build/ld" -Ttext 0x40000000 -o "$W/ours_ours.elf" "$W/start.o" "$W/o.o" "$RT/build/libosrt.a"
run() { timeout 60 "$QEMU" -M virt -cpu cortex-a7 -m 256 -nographic -semihosting -net none -kernel "$1" > "$2" 2>&1 || { echo "FAIL: $1 exited $?"; cat "$2"; exit 1; }; }
run "$W/ref.elf" "$W/ref.log"; run "$W/gcc_ours.elf" "$W/go.log"; run "$W/ours_ours.elf" "$W/oo.log"
ok=1
for v in go oo; do
	if ! diff -u "$W/ref.log" "$W/$v.log" > "$W/$v.diff"; then ok=0; echo "FAIL: $v differs from GCC+libgcc:"; cat "$W/$v.diff"; fi
done
[ $ok = 1 ] && echo "PASS runtime: $(wc -l < "$W/ref.log") checks identical — GCC+libgcc = GCC+libosrt = ours+libosrt" || exit 1
