#!/bin/bash
# dwline.sh — .debug_line, byte for byte against GNU as, plus the end-to-end property that matters:
# an address maps back to the right source line.
#
# A line table is how an address becomes a file and line. Without one, a fault gives you a number and the
# only way back to the source is computing offsets by hand out of the image. as used to refuse the directive
# outright, with an error that said "no debug info".
#
# The byte comparison covers each encoding path: a one-byte "special" opcode (a small address advance and a
# small line delta together), an explicit advance when either is out of range, several files, several
# sections, columns, and repeated directives at one address. The header fields are GNU's for ARM -- DWARF 3,
# min_inst_len 2, line_base -5, line_range 14, opcode_base 13 -- and GNU splits a path into its directory
# table, which is the case gcc's absolute paths always hit.
# Env: GNU=<cross prefix>, AS=<our as>, CC=<our cc>, CPP=<our cpp>, LD=<our ld> (the last three optional).
set -u
: "${GNU:?}" "${AS:?}"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); echo "PASS $*"; }
no() { fail=$((fail+1)); echo "FAIL $*"; }

bytes() {   # bytes <name> <asm>
	local name=$1 src=$2
	printf '%s\n' "$src" > "$t/c.s"
	"${GNU}as" -o "$t/g.o" "$t/c.s" 2>"$t/ge" || { echo "SKIP $name (GNU as rejected it)"; return; }
	"$AS" -o "$t/o.o" "$t/c.s" 2>"$t/oe" || { no "$name: our as: $(head -1 "$t/oe")"; return; }
	rm -f "$t/g.dl" "$t/o.dl"
	"${GNU}objcopy" --dump-section ".debug_line=$t/g.dl" "$t/g.o" /dev/null 2>/dev/null
	"${GNU}objcopy" --dump-section ".debug_line=$t/o.dl" "$t/o.o" /dev/null 2>/dev/null
	[ -e "$t/g.dl" ] || : > "$t/g.dl"; [ -e "$t/o.dl" ] || : > "$t/o.dl"
	if cmp -s "$t/o.dl" "$t/g.dl"; then ok "$name: .debug_line identical to GNU as ($(stat -c%s "$t/g.dl") bytes)"
	else
		no "$name: .debug_line differs"
		echo "      gnu : $(xxd -p "$t/g.dl" | tr -d '\n' | head -c 100)"
		echo "      ours: $(xxd -p "$t/o.dl" | tr -d '\n' | head -c 100)"
	fi
}

bytes 'one special opcode'          '	.text
	.file 1 "a.c"
	.loc 1 1 0
	nop
	.loc 1 2 0
	nop'
bytes 'line delta out of range'     '	.text
	.file 1 "a.c"
	.loc 1 1 0
	nop
	.loc 1 900 0
	nop
	.loc 1 2 0
	nop'
bytes 'address gap too big for one byte' '	.text
	.file 1 "a.c"
	.loc 1 1 0
	nop
	.space 400
	.loc 1 2 0
	nop'
bytes 'two files'                   '	.text
	.file 1 "a.c"
	.file 2 "b.h"
	.loc 1 1 0
	nop
	.loc 2 50 0
	nop
	.loc 1 2 0
	nop'
bytes 'columns'                     '	.text
	.file 1 "a.c"
	.loc 1 1 0
	nop
	.loc 1 2 9
	nop
	.loc 1 3 0
	nop'
bytes 'two .loc at one address'     '	.text
	.file 1 "a.c"
	.loc 1 1 0
	.loc 1 5 0
	nop'
bytes 'negative line delta'         '	.text
	.file 1 "a.c"
	.loc 1 20 0
	nop
	.loc 1 18 0
	nop'
bytes 'two sections'                '	.text
	.file 1 "a.c"
	.loc 1 1 0
	nop
	.section .text.other, "ax"
	.loc 1 40 0
	nop
	nop'
bytes 'a path, which GNU splits into its directory table' '	.text
	.file 1 "/some/where/a.c"
	.loc 1 3 0
	nop'

# END TO END: our own compiler's -g output, through our as and ld, read back with addr2line.
if [ -n "${CC:-}" ] && [ -n "${CPP:-}" ] && [ -x "$CC" ] && [ -x "$CPP" ]; then
	cat > "$t/p.c" <<'EOF'
int helper(int x) {
	return x * 2;
}
int main(void) {
	int a = helper(21);
	return a;
}
EOF
	"$CPP" "$t/p.c" > "$t/p.i" 2>/dev/null
	"$CC" -g -o "$t/p.s" "$t/p.i" && "$AS" -o "$t/p.o" "$t/p.s" || { no "our cc/as could not build it with -g"; }
	if [ -f "$t/p.o" ]; then
		# every row's address must resolve to that row's line
		bad=0; n=0
		while read -r line addr; do
			[ -n "${addr:-}" ] || continue
			case "$addr" in 0x*) ;; *) continue ;; esac
			got=$("${GNU}addr2line" -e "$t/p.o" "$addr" | sed 's/.*://')
			n=$((n+1)); [ "$got" = "$line" ] || { bad=$((bad+1)); echo "      $addr: table says line $line, addr2line says $got"; }
		done < <("${GNU}readelf" --debug-dump=decodedline "$t/p.o" 2>/dev/null | awk '$2 ~ /^[0-9]+$/ && $3 ~ /^0x/{print $2, $3}')
		[ "$n" -gt 0 ] && [ "$bad" = 0 ] && ok "addr2line agrees with the table on all $n rows of our own -g output" \
		                               || no "addr2line disagreed on $bad of $n rows"
		# and the line must be the right one for a known statement
		a2=$("${GNU}addr2line" -e "$t/p.o" "$("${GNU}readelf" --debug-dump=decodedline "$t/p.o" 2>/dev/null | awk '$2=="2" && $3 ~ /^0x/{print $3; exit}')")
		case "$a2" in *p.c:2) ok "an address inside the first statement resolves to p.c:2" ;;
		              *) no "expected p.c:2, got '$a2'" ;; esac
	fi
	if [ -n "${LD:-}" ] && [ -x "$LD" ] && [ -f "$t/p.o" ]; then
		printf '.text\n.globl _start\n_start: bl main\n b .\n' > "$t/s.s"; "$AS" -o "$t/s.o" "$t/s.s"
		if "$LD" -Ttext 0x40000000 -o "$t/p.elf" "$t/s.o" "$t/p.o" 2>/dev/null; then
			la=$("${GNU}readelf" --debug-dump=decodedline "$t/p.elf" 2>/dev/null | awk '$2=="2" && $3 ~ /^0x/{print $3; exit}')
			case "$("${GNU}addr2line" -e "$t/p.elf" "$la")" in *p.c:2) ok "survives the link: a relocated address still resolves to p.c:2" ;;
			    *) no "after linking, $la resolved to '$("${GNU}addr2line" -e "$t/p.elf" "$la")'" ;; esac
		else no "our ld could not link the -g object"; fi
	fi
fi

echo "dwarf line table: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
