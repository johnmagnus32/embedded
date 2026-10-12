#!/usr/bin/env bash
# size-diff.sh — OUR size's output against GNU size's, byte for byte (stdout AND stderr, and the exit status),
# for every format/radix combination, over: a probe object whose sections cover each berkeley_sum class
# (executable, read-only, writable-with-contents, nobits, non-ALLOC), an archive, several files at once (the
# header is printed only for the first, and -t totals across them), a static executable, a shared object, and
# whatever real artifacts the builds left behind.
#   GNU=<cross prefix> SIZE=<our size> ./size-diff.sh      exit 0 = every comparison identical
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "${HERE}/../../.." && pwd)"
GNU=${GNU:-${ROOT}/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
SIZE=${SIZE:-${HERE}/../build/size}
[ -x "${GNU}size" ] || { echo "SKIP size-diff: no GNU size at ${GNU}size"; exit 0; }
W=$(mktemp -d); trap 'rm -rf $W' EXIT
pass=0; fail=0

# ---- inputs ---------------------------------------------------------------------------------------------
# one section per berkeley_sum class, so a misclassification cannot hide
cat > $W/p.s <<'S'
	.text
	.global code
code:	bx lr
	.section .rodata,"a"
	.word 1, 2, 3
	.data
	.word 4
	.bss
	.space 64
	.section .note.mine,"a",%note
	.word 7
	.section .debug_stuff,"",%progbits      @ not ALLOC: counts in neither
	.word 8
S
"${GNU}as" -march=armv7-a -o $W/p.o $W/p.s || exit 2
printf 'int main(void){return 0;}\n' > $W/m.c
printf 'int helper(int x){return x+1;}\n' > $W/h.c
"${GNU}gcc" -c -o $W/h.o $W/h.c || exit 2
"${GNU}ar" rcs $W/lib.a $W/p.o $W/h.o
S_DIR="${ROOT}/projects/gameboy-v3/image/build/qemu/libc/stage-musl-static"
inputs=($W/p.o $W/h.o $W/lib.a)
if [ -d "$S_DIR" ]; then
	"${GNU}gcc" --sysroot="$S_DIR" -static -o $W/prog $W/m.c $W/h.c 2>/dev/null && inputs+=($W/prog)
	"${GNU}gcc" --sysroot="$S_DIR" -shared -fPIC -o $W/lib.so $W/h.c 2>/dev/null && inputs+=($W/lib.so)
fi
for real in "${ROOT}/projects/gameboy-v3/image/build/qemu/linux/vmlinux" \
            "${ROOT}/kernel/build/virt/kernel.elf" "${ROOT}/bootloader/build/bootloader.elf"; do
	[ -f "${real}" ] && inputs+=("${real}")
done

# ---- compare --------------------------------------------------------------------------------------------
cmp_run() {   # cmp_run <args...>
	( "${GNU}size" "$@" > $W/g.out 2> $W/g.err; echo $? > $W/g.rc )
	( "${SIZE}"    "$@" > $W/o.out 2> $W/o.err; echo $? > $W/o.rc )
	if cmp -s $W/g.out $W/o.out && cmp -s $W/g.rc $W/o.rc; then pass=$((pass+1)); else
		fail=$((fail+1)); echo "FAIL size $*"
		[ "$(cat $W/g.rc)" = "$(cat $W/o.rc)" ] || echo "    exit: GNU $(cat $W/g.rc), ours $(cat $W/o.rc)"
		diff $W/g.out $W/o.out | head -8 | sed 's/^/    /'
	fi
}
opts=("" "-A" "-B" "-d" "-o" "-x" "-t" "--format=sysv" "--format=berkeley" "--radix=8" "--radix=16"
      "-A -d" "-A -o" "-A -x" "-B -x" "-t -x" "-t -o")
for f in "${inputs[@]}"; do
	for o in "${opts[@]}"; do
		if [ -z "$o" ]; then cmp_run "$f"; else read -r -a oa <<< "$o"; cmp_run "${oa[@]}" "$f"; fi
	done
done
# several files in one run: the header prints once, -t sums across them
cmp_run "${inputs[@]}"
cmp_run -t "${inputs[@]}"
cmp_run -A "${inputs[@]}"
cmp_run -t -x "${inputs[@]}"
# and the error paths
cmp_run /nonexistent-file
cmp_run $W/p.s          # not an object

echo "size-diff: $pass identical, $fail different over ${#inputs[@]} inputs"
[ $fail -eq 0 ]
