#!/usr/bin/env bash
# gc.sh — --gc-sections against GNU ld: the same GCC objects (-ffunction-sections -fdata-sections -funwind-tables)
# linked by both, under a -T script (KEEP, __start_/__stop_, .ARM.exidx) and the default one, with -u pulling an
# archive member; --print-gc-sections must name the same sections removed, the outputs define the same symbols, and
# the unwind index (.ARM.exidx: GNU merges equal neighbours and adds EXIDX_CANTUNWIND after uncovered code and at the
# end, after gc) must be byte-identical, merged and not (--no-merge-exidx-entries). Plus -Ttext with -T (the
# script's .text moved, as GNU) and the errors. Env: GNU=<cross prefix>, LD=<our ld>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }; no() { fail=$((fail+1)); echo "FAIL $*"; }
cat > $t/a.c <<'C'
extern int helper(int), lib_entry(int);
extern char __start_regs[], __stop_regs[];
static int unused_static(int x) { return x * 3; }
int unused_global(int x) { return unused_static(x) + helper(x); }
static int used_static(int x) { return x + 1; }
int used_data = 5, unused_data = 6;
const int used_ro = 7, unused_ro = 8;
__attribute__((section("regs"))) int reg_a = 1;
__attribute__((section("kept_by_script"))) int keepme = 2;
__attribute__((section(".text.reached_by_ptr"))) int by_ptr(int x) { return x; }
int (*fp)(int) = by_ptr;
extern int helper3(int);
int _start(void) { return used_static(used_data) + used_ro + helper(1) + helper3(2) + (int)(__stop_regs - __start_regs) + fp(0); }
C
cat > $t/b.c <<'C'
int helper(int x) { return x << 1; }
int helper_unused(int x) { return x >> 1; }
int helper3(int x) { return x << 3; }
C
cat > $t/lib.c <<'C'
int lib_entry(int x) { return x + 42; }
int lib_other(int x) { return x - 42; }
C
printf '.section .text.__aeabi_unwind_cpp_pr0,"ax"\n.global __aeabi_unwind_cpp_pr0\n__aeabi_unwind_cpp_pr0: bx lr\n.global __aeabi_unwind_cpp_pr1\n.section .text.pr1,"ax"\n__aeabi_unwind_cpp_pr1: bx lr\n' > $t/pr.s
for f in a b lib; do "${GNU}gcc" -O1 -marm -ffunction-sections -fdata-sections -funwind-tables -fno-common -c -o $t/$f.o $t/$f.c || exit 2; done
"${GNU}as" -o $t/pr.o $t/pr.s; rm -f $t/lib.a; "${GNU}ar" rcs $t/lib.a $t/lib.o
cat > $t/s.ld <<'L'
ENTRY(_start)
SECTIONS {
  . = 0x10000;
  .text : { *(.text*) }
  .ARM.exidx : { *(.ARM.exidx*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) KEEP(*(kept_by_script)) }
  regs : { *(regs) }
  .bss : { *(.bss*) }
}
L
removed() { sed -n "s/.*: removing unused section '\([^']*\)' in file '\(.*\)'$/\1 \2/p" "$1" | sort; }
syms() { "${GNU}nm" "$1" | awk '$2 != "a" && $2 != "N" {print $2, $3}' | grep -v ' \$[atd]' | sort; }
exidx() { "${GNU}objcopy" -O binary -j .ARM.exidx "$1" /dev/stdout | od -An -tx4 | tr -s ' \n' ' '; }
for mode in script default nomerge; do
  if [ $mode = default ]; then opts=(-e _start); elif [ $mode = nomerge ]; then opts=(-T $t/s.ld --no-merge-exidx-entries); else opts=(-T $t/s.ld); fi
  args=(--gc-sections --print-gc-sections -u lib_entry "${opts[@]}" $t/a.o $t/b.o $t/pr.o $t/lib.a)
  "${GNU}ld" --no-warn-rwx-segments "${args[@]}" -o $t/g.elf 2>$t/g.err || { no "$mode: GNU ld failed: $(head -2 $t/g.err)"; continue; }
  "$LD" --no-warn-rwx-segments "${args[@]}" -o $t/o.elf 2>$t/o.err || { no "$mode: our ld failed: $(head -2 $t/o.err)"; continue; }
  if diff <(removed $t/g.err) <(removed $t/o.err) >$t/d; then ok; else no "$mode: removed sections differ (< GNU, > ours):"; sed 's/^/    /' $t/d | head -10; fi
  if diff <(syms $t/g.elf) <(syms $t/o.elf) >$t/d; then ok; else no "$mode: defined symbols differ (< GNU, > ours):"; sed 's/^/    /' $t/d | head -10; fi
  [ "$(removed $t/o.err | wc -l)" -ge 8 ] && ok || no "$mode: expected at least 8 sections removed, got $(removed $t/o.err | wc -l)"
  g=$(exidx $t/g.elf); o=$(exidx $t/o.elf)
  [ -n "$g" ] && [ "$g" = "$o" ] && ok || { no "$mode: .ARM.exidx differs from GNU's:"; echo "    gnu :$g"; echo "    ours:$o"; }
done
# -Ttext with -T: GNU's address statement moves the script's .text (and what follows it)
"${GNU}ld" --no-warn-rwx-segments -T $t/s.ld -Ttext 0x200000 --gc-sections -u lib_entry $t/a.o $t/b.o $t/pr.o $t/lib.a -o $t/g.elf 2>/dev/null
"$LD" --no-warn-rwx-segments -T $t/s.ld -Ttext 0x200000 --gc-sections -u lib_entry $t/a.o $t/b.o $t/pr.o $t/lib.a -o $t/o.elf 2>$t/o.err
addr() { "${GNU}readelf" -SW "$1" | sed -n 's/.* \.text *PROGBITS *\([0-9a-f]*\) .*/\1/p'; }
ga=$(addr $t/g.elf); oa=$(addr $t/o.elf)
[ -n "$ga" ] && [ "$ga" = "$oa" ] && [ "$ga" = 00200000 ] && ok || no "-Ttext with -T: .text at '$oa', GNU's at '$ga' ($(head -1 $t/o.err))"
"$LD" -T $t/s.ld --section-start=.nosuch=1000 $t/a.o -o $t/x.elf 2>$t/e && no "--section-start of a missing section succeeded" \
  || { grep -q "no output section of that name" $t/e && ok || no "--section-start missing: '$(cat $t/e)'"; }
echo "ld gc-sections: $pass passed, $fail failed"
[ $fail -eq 0 ]
