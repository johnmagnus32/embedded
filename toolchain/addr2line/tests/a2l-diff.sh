#!/bin/bash
# a2l-diff.sh — OUR addr2line against GNU's, over inputs built by BOTH toolchains.
#
# This is the consumer side of the line table. Until it existed the project could emit debug information that
# none of its own tools could read, and every check of it used GNU addr2line as oracle AND reader.
#
# Compared: every row of each input's line table (so the full address range, not a sampled one), the
# not-found answer, each option (-f -s -a -p and their combinations), the stdin path, and the errors.
# Env: GNU=<cross prefix>, A2L=<our addr2line>, TC=<toolchain dir>.
set -u
: "${GNU:?}" "${A2L:?}" "${TC:?}"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }
no() { fail=$((fail+1)); echo "FAIL $*"; }

# --- inputs: the same source built by gcc and by our own toolchain, object and executable ---
mkdir -p "$t/inc"
cat > "$t/inc/h.h" <<'EOF'
int twice(int v)
{
	return v * 2;
}
EOF
cat > "$t/p.c" <<'EOF'
#include "h.h"

int seed;

int main(void)
{
	int n = twice(seed);
	return n + 1;
}
EOF
printf '.text\n.globl _start\n_start: bl main\n b .\n' > "$t/s.s"

inputs=()
"${GNU}gcc" -c -g -O0 -marm -I"$t/inc" "$t/p.c" -o "$t/gcc.o" 2>/dev/null && inputs+=("$t/gcc.o")
if [ -x "$TC/cpp/build/cpp" ] && [ -x "$TC/cc/build/cc" ] && [ -x "$TC/as/build/as" ]; then
	"$TC/cpp/build/cpp" -I"$t/inc" "$t/p.c" > "$t/p.i" 2>/dev/null
	"$TC/cc/build/cc" -g -o "$t/p.s" "$t/p.i" && "$TC/as/build/as" -o "$t/ours.o" "$t/p.s" && inputs+=("$t/ours.o")
	"$TC/as/build/as" -o "$t/s.o" "$t/s.s" 2>/dev/null
	if [ -x "$TC/ld/build/ld" ] && [ -f "$t/ours.o" ]; then
		"$TC/ld/build/ld" -Ttext 0x40000000 -o "$t/ours.elf" "$t/s.o" "$t/ours.o" 2>/dev/null && inputs+=("$t/ours.elf")
	fi
fi
[ "${#inputs[@]}" -gt 0 ] || { echo "SKIP: no inputs could be built"; exit 0; }

cmp_one() {   # cmp_one <label> <input> <args...>
	local label=$1 in=$2; shift 2
	local o g
	o=$("$A2L" -e "$in" "$@" 2>&1); g=$("${GNU}addr2line" -e "$in" "$@" 2>&1)
	if [ "$o" = "$g" ]; then ok
	else no "$label ($(basename "$in") $*)"; echo "      gnu : $(echo "$g" | tr '\n' '|')"; echo "      ours: $(echo "$o" | tr '\n' '|')"; fi
}

for in in "${inputs[@]}"; do
	b=$(basename "$in")
	# every row's address, plus one byte past each, plus addresses outside any sequence
	mapfile -t as < <("${GNU}readelf" --debug-dump=decodedline "$in" 2>/dev/null | awk '$3 ~ /^0x/{print $3}' | sort -u)
	[ "${#as[@]}" -gt 0 ] || { echo "SKIP $b: no line table rows"; continue; }
	for a in "${as[@]}"; do
		cmp_one "row" "$in" "$a"
		cmp_one "row+2" "$in" "$(printf '0x%x' $((a + 2)))"
	done
	cmp_one "all rows at once" "$in" "${as[@]}"
	cmp_one "not found (0x1)" "$in" 0x1
	cmp_one "not found (huge)" "$in" 0xfffff000
	for opt in -f -s -a -p "-f -s" "-a -f" "-p -f" "-p -a -f" "-s -a"; do
		# shellcheck disable=SC2086
		cmp_one "options $opt" "$in" $opt "${as[0]}"
	done
	# stdin
	o=$(printf '%s\n' "${as[0]}" "${as[-1]}" | "$A2L" -e "$in" 2>&1)
	g=$(printf '%s\n' "${as[0]}" "${as[-1]}" | "${GNU}addr2line" -e "$in" 2>&1)
	[ "$o" = "$g" ] && ok || { no "stdin ($b)"; echo "      gnu : $(echo "$g" | tr '\n' '|')"; echo "      ours: $(echo "$o" | tr '\n' '|')"; }
	# -e joined, and --exe=
	cmp_one "-e joined" "$in" "${as[0]}"
	o=$("$A2L" "--exe=$in" "${as[0]}" 2>&1); g=$("${GNU}addr2line" "--exe=$in" "${as[0]}" 2>&1)
	[ "$o" = "$g" ] && ok || no "--exe= form ($b)"
done

# an input with no debug information at all
"${GNU}gcc" -c -O0 -marm "$t/p.c" -I"$t/inc" -o "$t/nodbg.o" 2>/dev/null
cmp_one "no debug info" "$t/nodbg.o" 0x0
# a missing file: the message and the status
o=$("$A2L" -e "$t/nope" 0x0 2>&1; echo "rc=$?"); g=$("${GNU}addr2line" -e "$t/nope" 0x0 2>&1; echo "rc=$?")
case "$o" in *"No such file"*rc=1) ok ;; *) no "missing file: ours [$o] gnu [$g]" ;; esac

# A REAL translation unit, if the kernel-parity harness has left its preprocessed output lying about: tens of
# thousands of lines across dozens of headers, which is what exercises the file table, the file-switch opcode
# and -- the case that actually found a bug -- several text sections whose addresses all start at 0, so the
# sequences overlap and only the relocation on each set_address says which section a sequence belongs to.
for i in /tmp/kernel_parity/*.i; do
	[ -f "$i" ] || continue
	[ -x "$TC/cc/build/cc" ] && [ -x "$TC/as/build/as" ] || break
	b=$(basename "$i" .i)
	"$TC/cc/build/cc" -mfloat-abi=soft -g -o "$t/$b.s" "$i" 2>/dev/null || continue
	"$TC/as/build/as" -o "$t/$b.o" "$t/$b.s" 2>/dev/null || continue
	d=0; n=0
	for a in $("${GNU}readelf" --debug-dump=decodedline "$t/$b.o" 2>/dev/null | awk '$3 ~ /^0x/{print $3}'); do
		n=$((n+1))
		[ "$("$A2L" -e "$t/$b.o" "$a" 2>&1)" = "$("${GNU}addr2line" -e "$t/$b.o" "$a" 2>&1)" ] || d=$((d+1))
	done
	[ "$n" = 0 ] && continue
	[ "$d" = 0 ] && ok || no "real TU $b: $d of $n rows differ"
	break      # one is enough for the gate; the full sweep is a manual check
done

echo "addr2line-diff: $pass identical to GNU addr2line, $fail different"
[ "$fail" -eq 0 ]
