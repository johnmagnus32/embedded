#!/usr/bin/env bash
# gnu_diff.sh — differential check against GCC: every torture/diff/*.c is built by GCC (-O0) and by our cpp/cc/as/ld,
# run on qemu (semihosting; printf through the torture runtime), and the two outputs must be identical. For features
# whose results are easier to compare than to predict (layouts, byte images, vector lanes, complex arithmetic).
# Needs the torture runtime (`make torture` once) and the reference cross-gcc.    exit 0 = all identical
set -u
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$R/toolchain
X=${GNU:-$R/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
RT=${CTORTURE_WORK:-/tmp/ctorture}/rt
[ -f "$RT/crt.o" ] && [ -f "$RT/crt_gnu.o" ] || { echo "gnu_diff: no torture runtime in $RT (run make torture first)"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
QEMU="qemu-system-arm -M virt -cpu cortex-a7 -m 256 -nographic -semihosting -net none -kernel"
INC="-nostdinc -isystem $R/libc/include -isystem $R/kernel/include/uapi"
pass=0; fail=0
for src in "$HERE"/diff/*.c; do
	n=$(basename "$src" .c)
	${X}gcc -O0 -marm -mcpu=cortex-a7 -w -fno-builtin-printf $INC -c -o "$W/$n.g.o" "$src" \
	  && ${X}ld -z noexecstack -Ttext=0x40000000 -o "$W/$n.g.elf" "$RT/crt_gnu.o" "$W/$n.g.o" $RT/{string,stdlib,stdio,printf,malloc,rt}.gnu.o "$(${X}gcc -print-libgcc-file-name)" \
	  || { echo "FAIL $n: GCC build"; fail=$((fail+1)); continue; }
	timeout 20 $QEMU "$W/$n.g.elf" > "$W/$n.gnu.txt" 2>/dev/null
	${X}gcc -E $INC -w -o "$W/$n.i" "$src" && "$T/cc/build/cc" -o "$W/$n.s" "$W/$n.i" && "$T/as/build/as" -o "$W/$n.o" "$W/$n.s" \
	  && "$T/ld/build/ld" -Ttext 0x40000000 -o "$W/$n.elf" "$RT/crt.o" "$W/$n.o" $RT/{string,stdlib,stdio,printf,malloc,rt}.ours.o "$T/rt/build/libosrt.a" \
	  || { echo "FAIL $n: our build"; fail=$((fail+1)); continue; }
	timeout 20 $QEMU "$W/$n.elf" > "$W/$n.ours.txt" 2>/dev/null
	if [ -s "$W/$n.gnu.txt" ] && cmp -s "$W/$n.gnu.txt" "$W/$n.ours.txt"; then pass=$((pass+1));
	else echo "FAIL $n: output differs from GCC's"; diff "$W/$n.gnu.txt" "$W/$n.ours.txt" | head -6 | sed 's/^/    /'; fail=$((fail+1)); fi
done
echo "gnu_diff: $pass identical, $fail differ (torture/diff/*.c vs GCC)"
[ $fail -eq 0 ]
