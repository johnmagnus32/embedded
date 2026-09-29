#!/usr/bin/env bash
# abi.sh — AAPCS / AAPCS-VFP interoperability with GCC: the functions in abi/callee.c and the calls in
# abi/caller.c are each built by GCC and by our cc, linked in all four combinations and run on qemu. Every
# case (FP args incl. back-filling and the stack spill past s15, HFA args/returns, variadic doubles, a struct
# split across r3 and the stack; vcallee/vcaller.c: generic vectors; acallee/acaller.c: doubleword alignment of
# over-aligned typedefs and structs) must give the same answer both ways, or our calling convention is wrong.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); T=$ROOT/toolchain
X=${GNU:-$ROOT/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
RT=${CTORTURE_WORK:-/tmp/ctorture}/rt; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
[ -f "$RT/crt_gnu.o" ] || { echo "abi: run 'make torture' first (builds the runtime in $RT)"; exit 1; }
build() {   # build <gcc|ours> <name>
	if [ "$1" = gcc ]; then "${X}gcc" -O2 -marm -mcpu=cortex-a7 -mfloat-abi=hard -ffreestanding -c -o "$W/$2.o" "$HERE/abi/$2.c"
	else "${X}gcc" -E -o "$W/$2.i" "$HERE/abi/$2.c" && "$T/cc/build/cc" -o "$W/$2.s" "$W/$2.i" && "$T/as/build/as" -o "$W/$2.o" "$W/$2.s"; fi
}
fail=0
for pair in "callee caller" "vcallee vcaller" "acallee acaller"; do set -- $pair
for callee in gcc ours; do for caller in gcc ours; do
	build "$callee" "$1"; build "$caller" "$2"
	"$T/ld/build/ld" -Ttext 0x40000000 -o "$W/m.elf" "$RT/crt_gnu.o" "$W/$2.o" "$W/$1.o" $(ls "$RT"/*.ours.o) "$T/rt/build/libosrt.a"
	set +e; timeout 10 qemu-system-arm -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel "$W/m.elf"; rc=$?; set -e
	if [ "$rc" = 255 ]; then echo "PASS $1=$callee $2=$caller"; else echo "FAIL $1=$callee $2=$caller: $rc (want 255: one bit per case)"; fail=1; fi
done; done; done
exit $fail
