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
# cc runs -mfloat-abi=soft, as the kernel is built (no VFP code/ABI attributes)
CC=$ROOT/toolchain/cc/build/cc; AS=$ROOT/toolchain/as/build/as
W=${WORK:-/tmp/kernel_parity}; mkdir -p "$W"
dirs=${*:-lib crypto kernel mm fs}

symtab() {   # "value size type bind visibility section-name name", section indices resolved to names
	"${GNU}readelf" -SW "$1" | awk '/^ +\[ *[0-9]+\]/{gsub(/[\[\]]/," "); print $1, $2}' > "$1.secmap"
	"${GNU}readelf" -sW "$1" | awk 'NR==FNR{m[$1]=$2; next} FNR>3 && $1 ~ /:$/ {n=($7 in m)?m[$7]:$7; if (n==".ARM.attributes") next; print $2, $3, $4, $5, $6, n, $8}' "$1.secmap" -
}
shdrs() {   # "name type flags align" per section, sorted (section ORDER may differ: .rel.X placement)
	"${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *[0-9]*\] //p' | awk '$1 != "" && $1 !~ /^NULL/ { fl = (NF == 10) ? $7 : "-"; print $1, $2, fl, $NF }' | sort
}
relocs() { "${GNU}readelf" -rW "$1" | awk -v s=".rel$2" 'index($0,"\x27" s "\x27"){p=1;next} /^Relocation section/{p=0} p&&/R_ARM/{print $3}' | sort | uniq -c; }

total=0; same=0; skipped=0
MIN=${MIN:-100}   # a floor, because the verdict below is `same == total` and 0 == 0 passes
cd "$KDIR" || { echo "kernel parity: no kernel tree at $KDIR (build one: cd projects/gameboy-v3/image && MACHINE=qemu make linux)" >&2; exit 2; }
for d in $dirs; do
	for o in "$d"/.*.o.cmd; do
		[ -e "$o" ] || continue
		b=$(basename "$o" .o.cmd); b=${b#.}; [ -f "$d/$b.c" ] || continue
		f=$W/$d-$b
		# ONE line, and the recorded flags the reference compiler will not take:
		#  - head -1, because a .o.cmd also carries source_/deps_ assignments. eval'ing all 286 lines lets the
		#    LAST one set the status, so a failed gcc -E reports success (kernel_parity_S.sh:22 already does this).
		#  - --param=allow-store-data-races=0 is in every recorded line because os-cc accepts ANY --param=;
		#    gcc 13.3 rejects it outright, so it has to come back out before the reference sees the line.
		pp=$(sed 's/^savedcmd_[^:]*:= //' "$o" | head -1 \
		     | sed -e "s#^[^ ]*gcc#${GNU}gcc#" -e 's# -c # -E #' -e "s# -o [^ ]*\.o # -o $f.i #" \
		           -e 's/--param=allow-store-data-races=[01]//')
		# The reference must actually have produced a non-empty .i: a silent skip here is what made this
		# whole gate read 0/116 "cc failed" while testing nothing.
		if ! eval "$pp" >"$f.pperr" 2>&1 || [ ! -s "$f.i" ]; then
			echo "$d/$b: SKIP (reference cpp: $(head -1 "$f.pperr"))"; skipped=$((skipped + 1)); continue
		fi
		total=$((total + 1))
		"$CC" -mfloat-abi=soft -o "$f.s" "$f.i" 2>"$f.err" || { echo "$d/$b: cc failed: $(head -1 "$f.err")"; continue; }
		"$AS" -o "$f.ours.o" "$f.s" 2>"$f.err" || { echo "$d/$b: our as failed: $(head -1 "$f.err")"; continue; }
		"${GNU}as" -mcpu=cortex-a7 -o "$f.gnu.o" "$f.s" 2>"$f.err" || { echo "$d/$b: GNU as failed: $(grep -m1 Error "$f.err")"; continue; }
		bad=""
		# .ARM.* IS compared: .ARM.exidx/.ARM.extab are the only coverage cc's .fnstart/.save/.setfp/.pad
		# emission can get, and .ARM.attributes is what a float-ABI-aware consumer reads.
		# The name comes out with sed, NOT awk's $2: readelf -SW pads a single-digit index as "[ 1]", so $2
		# is "1]" for every section below index 10 — this gate never once compared .text, .data, .bss,
		# .text.fixup, __ex_table or .rodata.str, and `objcopy -j '1]'` quietly produced nothing on both
		# sides, so those comparisons passed vacuously. shdrs() below always did it this way.
		for sec in $("${GNU}readelf" -SW "$f.gnu.o" | sed -n 's/^ *\[ *[0-9][0-9]*\] \([^ ]*\).*/\1/p' \
		             | awk '$0 != "" && $0 !~ /^\.(rel|symtab|strtab|shstrtab|note|comment)/' | sort -u); do
			# --dump-section, NOT -O binary -j: -j writes only ALLOCATED content, so it yields zero bytes
			# for .ARM.attributes (and .comment, and every .debug_*) and the comparison is vacuous.
			# Remove first: --dump-section leaves the file untouched when the section is absent, which would
			# otherwise compare the PREVIOUS section's bytes. Absent-on-one-side must read as empty, so a
			# section GNU emits and we do not is a real difference rather than a silent pass.
			rm -f "$f.a" "$f.b"
			"${GNU}objcopy" --dump-section "$sec=$f.a" "$f.ours.o" /dev/null 2>/dev/null
			"${GNU}objcopy" --dump-section "$sec=$f.b" "$f.gnu.o" /dev/null 2>/dev/null
			[ -e "$f.a" ] || : > "$f.a"; [ -e "$f.b" ] || : > "$f.b"
			cmp -s "$f.a" "$f.b" || bad="$bad $sec(bytes)"
			[ "$(relocs "$f.ours.o" "$sec")" = "$(relocs "$f.gnu.o" "$sec")" ] || bad="$bad $sec(relocs)"
		done
		diff -q <(symtab "$f.ours.o") <(symtab "$f.gnu.o") >/dev/null || bad="$bad symtab"
		diff -q <(shdrs "$f.ours.o") <(shdrs "$f.gnu.o") >/dev/null || bad="$bad shdrs"
		if [ -z "$bad" ]; then same=$((same + 1)); else echo "$d/$b: DIFF:$bad"; fi
	done
done
echo "kernel parity: $same/$total objects identical to GNU as (section bytes + relocs + symbol table + section headers), $skipped skipped"
if [ "$total" -lt "$MIN" ]; then
	echo "kernel parity: only $total objects were testable (floor: $MIN) — this gate asserts nothing at that count" >&2
	exit 1
fi
[ "$same" = "$total" ]
