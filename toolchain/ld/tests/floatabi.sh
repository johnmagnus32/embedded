#!/bin/bash
# floatabi.sh — the link's float ABI must be self-consistent, and recorded in e_flags.
#
# Two objects that disagree about how floating-point arguments are passed cannot call each other: one puts a
# double in a VFP register, the other in r0/r1. That is wrong arithmetic, not a crash, so the link must be
# refused. Our ld used to accept it silently (and hardcoded e_flags, so the output did not even say which ABI
# it was).
#
# Every expectation here is GNU ld's measured behaviour, not a reading of the spec, because the rule is not
# the obvious one: participation in the check is decided by Tag_ABI_FP_number_model, NOT by carrying
# Tag_ABI_VFP_args. A hand-written assembly object declares no FP model, so it links into either kind of
# program; a C object built -mfloat-abi=soft declares one, so it will not link into a hard-float program --
# even though neither object carries the VFP tag at all. Of Tag_ABI_FP_number_model, Tag_ABI_FP_denormal,
# Tag_ABI_FP_exceptions, Tag_FP_arch and an explicit Tag_ABI_VFP_args=0, only the first makes GNU enforce it.
#
# Env: GNU=<cross prefix>, LD=<our ld>.
set -u
: "${GNU:?}" "${LD:?}"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); echo "PASS $*"; }
no() { fail=$((fail+1)); echo "FAIL $*"; }

mk() { # mk NAME SOURCE FLAGS...
	local n=$1 src=$2; shift 2
	printf '%s\n' "$src" > "$t/$n.c"
	"${GNU}gcc" -c -O2 -marm -mcpu=cortex-a7 "$@" "$t/$n.c" -o "$t/$n.o"
}
mk nofp      'int nofp(int x){return x+1;}'        -mfloat-abi=soft
mk nofp_hard 'int nofph(int x){return x+1;}'       -mfloat-abi=hard -mfpu=neon-vfpv4
mk fp_soft   'double fsoft(double x){return x;}'   -mfloat-abi=soft
mk fp_softfp 'double fsfp(double x){return x;}'    -mfloat-abi=softfp -mfpu=neon-vfpv4
mk fp_hard   'double fhard(double x){return x;}'   -mfloat-abi=hard -mfpu=neon-vfpv4
printf '.text\n.globl _start\n_start: b .\n' > "$t/st.s"
"${GNU}as" -mcpu=cortex-a7 -o "$t/st.o" "$t/st.s"

# the pairs, and GNU's verdict for each (st.o is in every link and must never affect the outcome)
while read -r a b want; do
	"$LD" -Ttext 0x40000000 -o "$t/o.elf" "$t/st.o" "$t/$a.o" "$t/$b.o" 2>"$t/err"; rc=$?
	got=$([ $rc -eq 0 ] && echo accept || echo reject)
	if [ "$got" = "$want" ]; then
		if [ "$want" = reject ] && ! grep -q 'uses VFP register arguments' "$t/err"; then
			no "$a + $b: rejected, but not for the float ABI: $(head -1 "$t/err")"
		else ok "$a + $b -> $got (as GNU ld)"; fi
	else no "$a + $b -> $got, GNU ld says $want${rc:+ ($(head -1 "$t/err"))}"; fi
done <<'EOF'
nofp      nofp_hard reject
nofp      fp_soft   accept
nofp      fp_softfp accept
nofp      fp_hard   reject
nofp_hard fp_soft   reject
nofp_hard fp_softfp reject
nofp_hard fp_hard   accept
fp_soft   fp_softfp accept
fp_soft   fp_hard   reject
fp_softfp fp_hard   reject
EOF

# a bare assembly object declares no FP model, so it constrains nothing either way
for other in fp_hard fp_soft; do
	"$LD" -Ttext 0x40000000 -o "$t/o.elf" "$t/st.o" "$t/$other.o" 2>/dev/null \
		&& ok "a no-FP-model object links with $other (it makes no ABI claim)" \
		|| no "a no-FP-model object was refused against $other"
done

# e_flags must say which ABI the output is, as GNU's does
eflags() { "${GNU}readelf" -h "$1" | sed -n 's/.*Flags:[[:space:]]*\([^,]*\).*/\1/p'; }
for kind in "fp_hard:0x5000400:hard" "fp_soft:0x5000200:soft"; do
	o=${kind%%:*}; rest=${kind#*:}; want=${rest%%:*}; label=${rest##*:}
	"$LD"      -Ttext 0x40000000 -o "$t/o.elf" "$t/st.o" "$t/$o.o" 2>/dev/null
	"${GNU}ld" -Ttext 0x40000000 -o "$t/g.elf" "$t/st.o" "$t/$o.o" 2>/dev/null
	go=$(eflags "$t/o.elf"); gg=$(eflags "$t/g.elf")
	[ "$go" = "$want" ] && [ "$gg" = "$want" ] \
		&& ok "e_flags $go marks the output $label-float, as GNU ld does" \
		|| no "e_flags for $label: ours $go, GNU $gg, want $want"
done

echo "ld float ABI: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
