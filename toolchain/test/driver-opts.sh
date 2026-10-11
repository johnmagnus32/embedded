#!/usr/bin/env bash
# driver-opts.sh — os-cc's option handling against the real gcc's, for the families where a build system depends on
# the exact answer: the -g family and -T (the linker script). -g is the one gcc's driver sorts into accepted /
# warned-through / rejected, each with its own wording (`gcc -gfoo` → "unrecognized debug output level 'foo'"). We
# produce no debug information yet,
# so a request for it is ACCEPTED with a warning — gcc's own answer when it can't give the format asked for
# (`gcc -gstabs`: warning, exit 0, no .debug_* sections) — and a project that passes -g unconditionally, as U-Boot's
# KBUILD_CFLAGS/AFLAGS do, builds. Malformed and out-of-range forms are errors in gcc's words.
#
#   GNU=<cross prefix>  CC=<installed os-cc>  ./driver-opts.sh
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; ROOT="$(cd "${HERE}/../.." && pwd)"
GNU=${GNU:-${ROOT}/projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-}
CC=${CC:-}
[ -x "${GNU}gcc" ] || { echo "SKIP driver-opts: no gcc at ${GNU}gcc"; exit 0; }
[ -n "${CC}" ] && [ -x "${CC}" ] || { echo "SKIP driver-opts: set CC to an INSTALLED os-cc (its tools live beside it)"; exit 0; }
t=$(mktemp -d); trap 'rm -rf $t' EXIT
printf 'int f(int x){int y=x+1;return y*2;}\n' > $t/a.c
printf '\t.text\n\t.global s\ns:\tbx lr\n' > $t/a.S
pass=0; fail=0
ok() { pass=$((pass+1)); }; no() { fail=$((fail+1)); echo "FAIL $*"; }
# Run a compiler on one option; print "rc|message" with the tool's own name prefix stripped.
run() { local cc=$1 opt=$2 out rc
	out=$("${cc}" "${opt}" -c -o $t/o.o $t/a.c 2>&1); rc=$?
	printf '%d|%s' "$rc" "$(printf '%s' "$out" | head -1 | sed -e 's/^[^ ]*gcc: //' -e 's/^os-cc: //' -e 's/^error: //')"
}

# 1. the forms both must ACCEPT (gcc: exit 0; ours: exit 0, with the no-debug-info warning)
for opt in -g -g0 -g1 -g2 -g3 -ggdb -ggdb0 -ggdb1 -ggdb2 -ggdb3 -gdwarf -gdwarf-2 -gdwarf-3 -gdwarf-4 -gdwarf-5; do
	g=$(run "${GNU}gcc" "$opt"); o=$(run "${CC}" "$opt")
	[ "${g%%|*}" = 0 ] || { no "$opt: gcc itself rejected it (${g#*|})"; continue; }
	[ "${o%%|*}" = 0 ] && ok || no "$opt: we reject what gcc accepts: ${o#*|}"
done

# 2. the forms both must REJECT, in the same words
for opt in -gfoo -g+ -glevel=2 -g4 -g10 -ggdb4 -gdwarf4 -gdwarf-99; do
	g=$(run "${GNU}gcc" "$opt"); o=$(run "${CC}" "$opt")
	[ "${g%%|*}" != 0 ] || { no "$opt: gcc accepts it, so it does not belong here"; continue; }
	if [ "${o%%|*}" = 0 ]; then no "$opt: we accept what gcc rejects"
	elif [ "${o#*|}" = "${g#*|}" ]; then ok
	else no "$opt: our message differs from gcc's:"; echo "    gcc : ${g#*|}"; echo "    ours: ${o#*|}"; fi
done

# 3. DECLARED DIVERGENCES: gcc warns through (retired formats) or accepts (knobs that tune debug output we don't
#    emit, or that would change the output files); we refuse, because accepting could only ever be a lie. Asserted,
#    so changing our mind here has to be deliberate.
for opt in -gstabs -gstabs+ -gcoff -gxcoff -gsplit-dwarf -gz -gcolumn-info -gno-column-info -gstrict-dwarf -gpubnames; do
	g=$(run "${GNU}gcc" "$opt"); o=$(run "${CC}" "$opt")
	[ "${g%%|*}" = 0 ] || { no "$opt: gcc rejects it too — move it to the shared-rejection list"; continue; }
	[ "${o%%|*}" != 0 ] && ok || no "$opt: we now accept it; decide whether that is honest and move it to list 1"
done

# 4. -g really produces nothing, and still produces a valid object
"${CC}" -g -c -o $t/g.o $t/a.c 2>/dev/null || no "-g: compile failed"
n=$("${GNU}readelf" -SW $t/g.o 2>/dev/null | grep -c debug)
[ "$n" = 0 ] && ok || no "-g: $n .debug_* sections emitted, but we claim none"
"${GNU}nm" $t/g.o | grep -q ' T f$' && ok || no "-g: the object lost its code"

# 5. -g0 is silent; a request warns exactly once (gcc warns once per invocation too)
[ -z "$("${CC}" -g0 -c -o $t/o.o $t/a.c 2>&1)" ] && ok || no "-g0 is not silent (it asks for nothing)"
w=$("${CC}" -g -c -o $t/o.o $t/a.c 2>&1 | grep -c 'no debug information')
[ "$w" = 1 ] && ok || no "-g warned $w times, want exactly 1"
# ...and not during a capability probe, where a build system reads only the answer
[ -z "$("${CC}" -g -print-file-name=include 2>&1 >/dev/null)" ] && ok || no "-g warns during a -print-* query"

# 6. an assembly input with -gdwarf-4 (U-Boot's KBUILD_AFLAGS): gcc hands its assembler --gdwarf-4; we have no line
#    table yet, so nothing is forwarded — but it must still assemble
"${CC}" -gdwarf-4 -c -o $t/s.o $t/a.S 2>/dev/null && "${GNU}nm" $t/s.o | grep -q ' T s$' && ok || no "-gdwarf-4 on a .S input"

# 7. -T <script>: the linker script, which gcc takes separate OR joined and forwards to ld. Our bootloader's
#    Makefile passes the separate form, so both must put the output where the script says. -Tbss=/-Tdata= are a
#    DECLARED DIVERGENCE: gcc forwards them, we refuse (a script is the one way we place sections).
cat > $t/s.ld <<'L'
ENTRY(_start)
SECTIONS { . = 0x28000; .text : { *(.text) } /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) } }
L
printf 'void _start(void){}\n' > $t/s.c
entry() { "${GNU}readelf" -h "$1" | sed -n 's/.*Entry point address: *//p'; }
for form in "-T $t/s.ld" "-T$t/s.ld"; do
  for cc in "${GNU}gcc" "${CC}"; do
    if "$cc" -nostdlib $form $t/s.c -o $t/s.out 2>$t/e && [ "$(entry $t/s.out)" = 0x28000 ]; then ok
    else no "'$form' with $(basename "$cc"): $(head -1 $t/e)"; fi
  done
done
"${CC}" -c -T $t/s.ld -o $t/s.o $t/s.c 2>/dev/null && ok || no "-T is ignored when only compiling (gcc ignores it)"
"${GNU}gcc" -nostdlib -Tbss=0x40000000 $t/s.c -o $t/s.out 2>/dev/null \
  && { "${CC}" -nostdlib -Tbss=0x40000000 $t/s.c -o $t/s.out 2>/dev/null && no "-Tbss= accepted; decide whether that is honest" || ok; } \
  || no "-Tbss=: gcc rejects it too — this divergence no longer exists"

# 8. A suffix we do not compile is LINKER INPUT, which is how gcc's driver decides it — musl names its shared
#    objects .lo and links them without a murmur. -x forces the language; -c warns and carries on, in gcc's words.
printf 'int helper(int x){return x+1;}\n' > $t/h.c
"${GNU}gcc" -c $t/h.c -o $t/h.lo
printf 'void _start(void){}\n' > $t/l.c
for cc in "${GNU}gcc" "${CC}"; do
  if "$cc" -nostdlib -T $t/s.ld $t/l.c $t/h.lo -o $t/l.out 2>$t/e && "${GNU}nm" $t/l.out | grep -q ' T helper$'; then ok
  else no "a .lo as linker input with $(basename "$cc"): $(head -1 $t/e)"; fi
done
g=$("${GNU}gcc" -c $t/h.lo -o $t/z.o 2>&1); grc=$?
o=$("${CC}" -c $t/h.lo -o $t/z.o 2>&1); orc=$?
[ $grc = 0 ] && [ $orc = 0 ] && ok || no "-c on a linker input: gcc exit $grc, ours $orc"
[ "$(printf '%s' "$o" | sed -e 's/^os-cc: //')" = "$(printf '%s' "$g" | sed -e 's/^[^ ]*gcc: //')" ] && ok \
  || { no "-c on a linker input: the warning differs"; echo "    gcc : $g"; echo "    ours: $o"; }
cp $t/h.c $t/src.lo   # the same suffix, but C: -x says so, and then it is compiled
"${CC}" -c -x c $t/src.lo -o $t/z.o 2>/dev/null && "${GNU}nm" $t/z.o | grep -q ' T helper$' && ok || no "-x c must compile a .lo as C"

echo "driver options: $pass passed, $fail failed"
[ $fail -eq 0 ]
