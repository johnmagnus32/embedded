#!/bin/bash
# objcmp.sh OURS.o GNU.o — the two objects must be the same object: every section's bytes and relocations (in .rel
# order, with offsets), the section headers (name, type, flags, alignment) and SHF_LINK_ORDER links, and the symbol
# table in order (mapping symbols included; section numbers shown by name). Prints what differs; exit 1 if anything.
# Section NUMBERING and file layout (GNU puts .rel.X right after X; BFD merges string-table suffixes) aren't compared.
#   GNU=<cross prefix> objcmp.sh ours.o gnu.o
set -u
: "${GNU:?}"; A=$1; B=$2
shdrs() { "${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *[0-9]*\] //p' | awk '$1!="NULL"{fl=(NF==10)?$7:"-"; print $1,$2,fl,$NF}' | sort; }
relocs() { "${GNU}readelf" -rW "$1" | awk -v s=".rel$2" 'index($0,"\x27" s "\x27"){p=1;next} /^Relocation section/{p=0} p&&/R_ARM/{print $1,$3,$5}'; }
symtab() {
	"${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *\([0-9]*\)\] \([^ ]*\).*/\1 \2/p' > "$1.sm"
	"${GNU}readelf" -sW "$1" | awk 'NR==FNR{m[$1]=$2;next} FNR>3 && $1~/:$/{n=($7 in m)?m[$7]:$7; print $2,$3,$4,$5,$6,n,$8}' "$1.sm" -
	rm -f "$1.sm"
}
links() {
	"${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *\([0-9]*\)\] \([^ ]*\).*/\1 \2/p' > "$1.num"
	"${GNU}readelf" -SW "$1" | sed -n 's/^ *\[ *[0-9]*\] //p' | awk '$0 ~ / AL /{print $1, $(NF-2)}' | awk 'NR==FNR{n[$1]=$2;next}{print $1, n[$2]}' "$1.num" - | sort
	rm -f "$1.num"
}
bad=""; t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
for sec in $("${GNU}readelf" -SW "$B" | sed -n 's/^ *\[ *[0-9]*\] //p' | awk '$1!~/^\.(rel|symtab|strtab|shstrtab)/ && $1!="NULL"{print $1}' | sort -u); do
	# --dump-section, NOT -O binary -j: -j writes only ALLOCATED content, so it yields zero bytes for
	# .ARM.attributes (which this loop does select), .comment and every .debug_* — a vacuous comparison.
	# Remove first, since --dump-section leaves the file alone when the section is absent.
	rm -f "$t/a" "$t/b"
	"${GNU}objcopy" --dump-section "$sec=$t/a" "$A" /dev/null 2>/dev/null
	"${GNU}objcopy" --dump-section "$sec=$t/b" "$B" /dev/null 2>/dev/null
	[ -e "$t/a" ] || : > "$t/a"; [ -e "$t/b" ] || : > "$t/b"
	cmp -s "$t/a" "$t/b" || bad="$bad $sec(bytes)"
	[ "$(relocs "$A" "$sec")" = "$(relocs "$B" "$sec")" ] || bad="$bad $sec(relocs)"
done
[ "$(shdrs "$A")" = "$(shdrs "$B")" ] || bad="$bad shdrs"
[ "$(links "$A")" = "$(links "$B")" ] || bad="$bad sh_link"
if ! diff -q <(symtab "$A") <(symtab "$B") >/dev/null; then bad="$bad symtab"; [ -n "${VERBOSE:-}" ] && diff <(symtab "$A") <(symtab "$B") | head -10 >&2; fi
[ -z "$bad" ] || { echo "$bad"; exit 1; }
