#!/bin/bash
# perline.sh <file.s>... — validation gaps per LINE. binutils' diagnostic tests pass/fail per FILE, so one rejected line
# hides every other line we wrongly accept. Assemble each instruction line alone (with the file's leading target
# directives) through our as and GNU as (at our target, -mcpu=cortex-a7); report lines GNU rejects but we accept.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
GNU=${GNU:-$ROOT/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
A=$ROOT/toolchain/as/build/as; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; total=0
for f in "$@"; do
	pre=$(grep -E '^\s*\.(syntax|arch|cpu|fpu|text|arm|align)' "$f"); n=0; gap=0
	while IFS= read -r l; do
		t=$(printf '%s' "$l" | sed 's/^\s*//; s/\s*@.*$//; s/\s*\/\*.*$//'); [ -z "$t" ] && continue
		case "$t" in .*|*:|\#*) continue;; esac
		printf '%s\nlbl: %s\n' "$pre" "$t" > "$T/l.s"; n=$((n + 1))
		"${GNU}as" -mcpu=cortex-a7 -o "$T/g.o" "$T/l.s" 2>/dev/null; g=$?; "$A" -o "$T/o.o" "$T/l.s" 2>/dev/null; o=$?
		[ $g -ne 0 ] && [ $o -eq 0 ] && { gap=$((gap + 1)); echo "  ACCEPTED (GNU rejects): $t"; }
	done < "$f"
	echo "$(basename "$f"): $n lines, $gap validation gaps"; total=$((total + gap))
done
[ "$total" -eq 0 ]
