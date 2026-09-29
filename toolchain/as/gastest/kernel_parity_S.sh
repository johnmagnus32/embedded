#!/bin/bash
# kernel_parity_S.sh — the kernel's ASSEMBLY sources through OUR as vs GNU as: every .S the kernel build assembles is
# preprocessed exactly as Kbuild did (its .o.cmd, -c -> -E), then assembled by both with the options GCC hands its
# assembler (-EL -march=armv7ve -mfloat-abi=soft -mfpu=vfp -meabi=5 + the -Wa ones, -I dirs), and every section — the
# EHABI .ARM.exidx/.ARM.extab unwind tables included — must match: bytes, relocations, symbol table, section headers.
# (kernel_parity.sh does the C side: our cc's output through both assemblers.)
#
#   KDIR=<kernel build tree with .*.o.cmd>  GNU=<cross prefix>  ./kernel_parity_S.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
KDIR=${KDIR:-$ROOT/projects/gameboy-v3/image/build/qemu/linux}
GNU=${GNU:-$ROOT/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
AS=$ROOT/toolchain/as/build/as
W=${WORK:-/tmp/kernel_parity_S}; rm -rf "$W"; mkdir -p "$W"
cd "$KDIR" || exit 2

one() {
	local o=$1 d b f pp asflags
	d=$(dirname "$o"); b=$(basename "$o" .o.cmd); b=${b#.}
	[ -f "$d/$b.S" ] || return 0
	f=$W/$(echo "$d/$b" | tr / _)
	pp=$(sed 's/^savedcmd_[^:]*:= //' "$o" | head -1)
	case "$pp" in *gcc*) ;; *) return 0 ;; esac
	asflags=$(echo "$pp" | tr ' ' '\n' | sed -n 's/^-Wa,//p' | tr ',' ' ')
	incs=$(echo "$pp" | tr ' ' '\n' | grep '^-I' | tr '\n' ' ')
	pp=$(echo "$pp" | sed -e "s#^[^ ]*gcc#${GNU}gcc#" -e 's# -c # -E #' -e "s# -o [^ ]*\.o # -o $f.s #" -e 's#-Wp,-MMD,[^ ]*##')
	eval "$pp" >/dev/null 2>&1 || { echo "SKIP $d/$b: preprocessing failed"; return 0; }
	local opts="$incs -EL -march=armv7ve -mfloat-abi=soft -mfpu=vfp -meabi=5 $asflags"
	"${GNU}as" $opts -o "$f.gnu.o" "$f.s" 2>"$f.gerr" || { echo "SKIP $d/$b: GNU as failed"; return 0; }
	if ! "$AS" $opts -o "$f.ours.o" "$f.s" 2>"$f.err"; then echo "FAIL $d/$b: our as: $(head -1 "$f.err")"; return 0; fi
	local bad; bad=$("$HERE/objcmp.sh" "$f.ours.o" "$f.gnu.o")
	if [ -z "$bad" ]; then echo "PASS $d/$b"; else echo "FAIL $d/$b:$bad"; fi
}
export -f one; export GNU AS W HERE
find . -name '.*.o.cmd' | sort | xargs -P "$(nproc)" -I{} bash -c 'one "$@"' _ {} > "$W/results"
grep -v '^PASS' "$W/results" | sort
p=$(grep -c '^PASS' "$W/results"); t=$(grep -c '^\(PASS\|FAIL\)' "$W/results")
echo "kernel .S parity: $p/$t assembly objects identical to GNU as (bytes + relocs + symtab + headers, .ARM.exidx/extab included)"
[ "$p" = "$t" ]
