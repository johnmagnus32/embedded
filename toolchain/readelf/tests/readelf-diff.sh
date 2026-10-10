#!/usr/bin/env bash
# readelf-diff.sh — OUR readelf's output against GNU readelf's, byte for byte (stdout AND stderr, and the exit
# status), for every option set we implement, over whatever the builds left behind: relocatable objects (including
# ones with GOT-relative and group relocations, ARM unwind tables, and mapping symbols), archives (plain and thin),
# a static executable, a PIE, a shared object with versioned symbols, the kernel's vmlinux and U-Boot's image.
# A path printed inside the output must be the same for both, so each input is dumped from the same directory
# under the same name.   exit 0 = every comparison identical
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "${HERE}/../../.." && pwd)"
GNU=${GNU:-${ROOT}/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
READELF=${READELF:-${HERE}/../build/readelf}
[ -x "${GNU}objdump" ] || { echo "SKIP objdump-diff: no GNU readelf at ${GNU}objdump"; exit 0; }
W=$(mktemp -d); trap 'rm -rf $W' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }
no() { fail=$((fail+1)); echo "FAIL $*"; }

# ---- inputs -------------------------------------------------------------------------------------------------
cat > $W/a.c <<'C'
extern int ext_fn(int), ext_var;
int g = 5, *gp = &g, arr[4] = { 1, 2, 3, 4 };
static int priv = 7;
const char ro[] = "ro";
char bss[32];
static int helper(int x) { return x + priv; }
int fn(int x) { return helper(x) + ext_fn(x) + ext_var + arr[x & 3]; }
int weak_fn(int) __attribute__((weak));
int weak_fn(int x) { return x; }
__attribute__((visibility("hidden"))) int hid(void) { return 1; }
__attribute__((visibility("protected"))) int prot(void) { return 2; }
C
cat > $W/b.c <<'C'
int ext_var = 9;
int ext_fn(int x) { return x * 3; }
int unused_b(int x) { return x - 1; }
C
printf '\t.text\n\t.global asm_sym\nasm_sym:\n\tnop\n\t.word 0x1234\n\t.thumb\n\t.global tsym\ntsym:\n\tnop\n\t.arm\n\t.section .note.mine,"a",%%note\n\t.word 1\n' > $W/m.S
"${GNU}gcc" -O1 -marm -funwind-tables -fno-common -c -o $W/a.o   $W/a.c  || exit 2
"${GNU}gcc" -O1 -marm -fPIC -fno-common -c -o $W/apic.o $W/a.c  || exit 2   # GOT-relative relocations
"${GNU}gcc" -O1 -marm -fno-common -c -o $W/b.o   $W/b.c  || exit 2
"${GNU}gcc" -O1 -marm -fPIC -fno-common -c -o $W/bpic.o $W/b.c  || exit 2
"${GNU}as" -march=armv7-a -o $W/m.o $W/m.S || exit 2                        # mapping symbols ($a/$d/$t)
"${GNU}ar" rcs $W/lib.a $W/a.o $W/b.o
(cd $W && "${GNU}ar" rcsT thin.a a.o b.o)
"${GNU}gcc" -O1 -marm -c -o $W/c.o $W/b.c
"${GNU}ld" --no-warn-rwx-segments -e fn -o $W/prog $W/a.o $W/b.o 2>/dev/null
"${GNU}ld" --no-warn-rwx-segments -shared -o $W/lib.so $W/apic.o $W/bpic.o 2>/dev/null
cat > $W/v.map <<'M'
V1 { global: fn; local: *; };
V2 { global: ext_fn; } V1;
M
"${GNU}ld" --no-warn-rwx-segments -shared --version-script $W/v.map -o $W/ver.so $W/apic.o $W/bpic.o 2>/dev/null

inputs=(a.o apic.o b.o m.o lib.a thin.a)
[ -f $W/prog ]   && inputs+=(prog)
[ -f $W/lib.so ] && inputs+=(lib.so)
[ -f $W/ver.so ] && inputs+=(ver.so)
# real artifacts, if a build left them where we can see them
for real in "${ROOT}/projects/gameboy-v3/image/build/qemu/linux/vmlinux" /tmp/ub-ours/u-boot /tmp/kbc/vmlinux; do
	[ -f "${real}" ] && { cp "${real}" "$W/$(basename "${real}")" && inputs+=("$(basename "${real}")"); }
done

# ---- compare ------------------------------------------------------------------------------------------------
# Each option set, over each input. Both tools run in $W on the same relative name, so any path they print matches.
opts=("-h" "-S" "-s" "-r" "-d" "-n" "-rW" "-SW" "-sW" "-h -S" "-r -d"
      "--file-header" "--section-headers" "--syms" "--relocs" "--dynamic" "--notes")
for f in "${inputs[@]}"; do
	for o in "${opts[@]}"; do
		( cd $W && "${GNU}readelf" $o "$f" > g.out 2> g.err; echo $? > g.rc )
		( cd $W && "${READELF}"    $o "$f" > o.out 2> o.err; echo $? > o.rc )
		if cmp -s $W/g.out $W/o.out && cmp -s $W/g.rc $W/o.rc; then ok
		else
			no "readelf $o $f"
			[ "$(cat $W/g.rc)" = "$(cat $W/o.rc)" ] || echo "    exit: GNU $(cat $W/g.rc), ours $(cat $W/o.rc) ($(head -1 $W/o.err))"
			diff $W/g.out $W/o.out | head -8 | sed 's/^/    /'
		fi
	done
done

echo "readelf-diff: $pass identical, $((fail)) different over ${#inputs[@]} inputs x ${#opts[@]} option sets"
[ $fail -eq 0 ]
