#!/bin/bash
# kernel_parity.sh — compile real kernel sources with OUR cc, assemble the result with BOTH our as and GNU as,
# and require every section's bytes, relocation types, and the symbol table (section-name resolved) to match.
# This is how the silent mapping-symbol / section-model / string-escape / immediate bugs were found.
#
#   KDIR=<kernel build tree with .*.o.cmd>  GNU=<binutils/gcc 2.42 cross prefix>  ./kernel_parity.sh [dir...]
# Defaults: the qemu build's kernel tree + cross toolchain; dirs = lib crypto kernel mm fs.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
KDIR=${KDIR:-$ROOT/projects/gameboy-v3/image/build/qemu/linux}
GNU=${GNU:-$ROOT/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
CC=$ROOT/toolchain/cc/build/cc; AS=$ROOT/toolchain/as/build/as
W=${WORK:-/tmp/kernel_parity}; mkdir -p "$W"
dirs=${*:-lib crypto kernel mm fs}

symtab() {   # "value size type bind section-name name", section indices resolved to names
	"${GNU}readelf" -SW "$1" | awk '/^ +\[ *[0-9]+\]/{gsub(/[\[\]]/," "); print $1, $2}' > "$1.secmap"
	"${GNU}readelf" -sW "$1" | awk 'NR==FNR{m[$1]=$2; next} FNR>3 && $1 ~ /:$/ {n=($7 in m)?m[$7]:$7; if (n==".ARM.attributes") next; print $2, $3, $4, $5, n, $8}' "$1.secmap" -
}
shdrs() {   # "name type flags align" per section, sorted (section ORDER may differ: .rel.X placement)
	"${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *[0-9]*\] //p' | awk '$1 != "" && $1 !~ /^NULL/ { fl = (NF == 10) ? $7 : "-"; print $1, $2, fl, $NF }' | sort
}
relocs() { "${GNU}readelf" -rW "$1" | awk -v s=".rel$2" 'index($0,"\x27" s "\x27"){p=1;next} /^Relocation section/{p=0} p&&/R_ARM/{print $3}' | sort | uniq -c; }

total=0; same=0
cd "$KDIR" || exit 2
for d in $dirs; do
	for o in "$d"/.*.o.cmd; do
		[ -e "$o" ] || continue
		b=$(basename "$o" .o.cmd); b=${b#.}; [ -f "$d/$b.c" ] || continue
		f=$W/$d-$b
		pp=$(sed 's/^savedcmd_[^:]*:= //' "$o" | sed -e "s#^[^ ]*gcc#${GNU}gcc#" -e 's# -c # -E #' -e "s# -o [^ ]*\.o # -o $f.i #")
		eval "$pp" >/dev/null 2>&1 || continue
		total=$((total + 1))
		"$CC" -o "$f.s" "$f.i" 2>"$f.err" || { echo "$d/$b: cc failed: $(head -1 "$f.err")"; continue; }
		"$AS" -o "$f.ours.o" "$f.s" 2>"$f.err" || { echo "$d/$b: our as failed: $(head -1 "$f.err")"; continue; }
		"${GNU}as" -mcpu=cortex-a7 -o "$f.gnu.o" "$f.s" 2>"$f.err" || { echo "$d/$b: GNU as failed: $(grep -m1 Error "$f.err")"; continue; }
		bad=""
		for sec in $("${GNU}readelf" -SW "$f.gnu.o" | awk '/\] [._a-zA-Z]/{n=$2; if (n!~/^\.(rel|symtab|strtab|shstrtab|note|comment)/ && n!~/^\.ARM\./) print n}' | sort -u); do
			"${GNU}objcopy" -O binary -j "$sec" "$f.ours.o" "$f.a" 2>/dev/null; "${GNU}objcopy" -O binary -j "$sec" "$f.gnu.o" "$f.b" 2>/dev/null
			cmp -s "$f.a" "$f.b" || bad="$bad $sec(bytes)"
			[ "$(relocs "$f.ours.o" "$sec")" = "$(relocs "$f.gnu.o" "$sec")" ] || bad="$bad $sec(relocs)"
		done
		diff -q <(symtab "$f.ours.o") <(symtab "$f.gnu.o") >/dev/null || bad="$bad symtab"
		diff -q <(shdrs "$f.ours.o") <(shdrs "$f.gnu.o") >/dev/null || bad="$bad shdrs"
		if [ -z "$bad" ]; then same=$((same + 1)); else echo "$d/$b: DIFF:$bad"; fi
	done
done
echo "kernel parity: $same/$total objects identical to GNU as (section bytes + relocs + symbol table + section headers)"
[ "$same" = "$total" ]
