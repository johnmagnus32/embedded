#!/bin/bash
# markers.sh — cpp must say where the text came from: `# <line> "<file>" [flag]`, GCC's line markers.
#
# Without them a compiler diagnostic can only name a line in the preprocessed temporary the driver then
# deletes, and no filename: a missing semicolon at widget.h:3 was reported as "(line 6)".
#
# This asserts the marker sequence for the USER's files against a hand-written expectation rather than
# byte-for-byte against `gcc -E`, deliberately: GCC also emits a `# 0 "<built-in>"` / `# 0 "<command-line>"`
# preamble and pulls in stdc-predef.h, none of which names a real file or means anything to a compiler. The
# flags are GCC's: 1 entering a file, 2 returning to one.
# Env: CPP=<our cpp>, GNU=<cross prefix> (optional, for the cross-check).
set -u
: "${CPP:?}"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); echo "PASS $*"; }
no() { fail=$((fail+1)); echo "FAIL $*"; }

mkdir -p "$t/inc"
printf 'struct w {\n\tint a;\n\tint b;\n};\n' > "$t/inc/w.h"
printf 'int pre;\n#include "w.h"\nint post;\n' > "$t/main.c"
printf 'int one;\n#include "w.h"\nint two;\n#include "w.h"\nint three;\n' > "$t/twice.c"

markers() { "$CPP" -I"$t/inc" "$1" 2>/dev/null | grep '^#'; }

# 1. the main file is announced first, then the include is entered and returned from, at the right lines
got=$(markers "$t/main.c" | sed "s#$t/#@/#g")
want='# 1 "@/main.c"
# 1 "@/inc/w.h" 1
# 3 "@/main.c" 2'
if [ "$got" = "$want" ]; then ok "marker sequence: open the main file, enter the include (flag 1), return after it (flag 2)"
else no "marker sequence:"; printf '    want: %s\n    got:  %s\n' "$(echo "$want" | tr '\n' '|')" "$(echo "$got" | tr '\n' '|')"; fi

# 2. the line a marker returns to is the line AFTER the #include, so what follows is numbered correctly
ret=$(markers "$t/main.c" | sed -n '3s/# \([0-9]*\).*/\1/p')
[ "$ret" = 3 ] && ok "the return marker names the line after the #include (3)" || no "return marker says line '$ret', want 3"

# 3. including the same file twice gives two enter/return pairs
n=$(markers "$t/twice.c" | grep -c '" 1$')
[ "$n" = 2 ] && ok "two includes of one header produce two 'entering' markers" || no "$n 'entering' markers for two includes, want 2"

# 4. -P must suppress every marker: Kbuild preprocesses linker scripts that way and a '#' would break them
[ "$("$CPP" -P -I"$t/inc" "$t/main.c" 2>/dev/null | grep -c '^#')" = 0 ] \
	&& ok "-P emits no markers at all" || no "-P emitted markers"

# 5. #line moves the reported position, and a marker records where it moved to
printf 'int a;\n#line 100 "elsewhere.c"\nint b;\n' > "$t/lineno.c"
if "$CPP" "$t/lineno.c" 2>/dev/null | grep -q '^# 100 "elsewhere.c"'; then ok "#line is reflected in a marker"
else no "#line produced no marker: $("$CPP" "$t/lineno.c" 2>/dev/null | grep '^#' | tr '\n' '|')"; fi

# 6. cross-check against GNU where available: the markers naming OUR files must agree exactly
if [ -n "${GNU:-}" ] && [ -x "${GNU}gcc" ]; then
	ours=$(markers "$t/main.c")
	# `# 0 "..."` is part of GCC's built-in preamble (it brackets <built-in> and <command-line>); line 0 is not
	# a position in anyone's file and a compiler has nothing to do with it, so it is not compared.
	theirs=$("${GNU}gcc" -E -I"$t/inc" "$t/main.c" 2>/dev/null | grep '^#' | grep -F "$t" | grep -v '^# 0 ')
	[ "$ours" = "$theirs" ] && ok "markers for the user's files are identical to GCC's" \
		|| { no "differ from GCC's:"; printf '    gnu : %s\n    ours: %s\n' "$(echo "$theirs" | tr '\n' '|')" "$(echo "$ours" | tr '\n' '|')"; }
else echo "SKIP GCC cross-check (no GNU prefix)"; fi

echo "cpp line markers: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
