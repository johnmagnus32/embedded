#!/usr/bin/env bash
# dynamic.sh — the dynamic tables against GNU ld's, from the same GCC objects: a -pie and a -shared output's .dynsym
# (a null entry, GNU's two section symbols, the same exported names), .hash's bucket count, the dynamic relocations
# — sorted into .rel.dyn (-z combreloc: RELATIVE first by address), or per section (-z nocombreloc: .rel<section>,
# each in GNU's order) — DT_RELCOUNT, DT_TEXTREL, .interp (and none under --no-dynamic-linker), and a U-Boot-style
# PIE whose .hash is byte-identical. Env: GNU=<cross prefix>, LD=<our ld>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
ok() { pass=$((pass+1)); }; no() { fail=$((fail+1)); echo "FAIL $*"; }
cat > $t/a.c <<'C'
extern int b_val, b_fn(int);
int a_val = 3, *a_ptr = &a_val, *b_ptr = &b_val;
int (*fps[])(int) = { b_fn, 0 };
const char *const names[] = { "one", "two", "three" };
int a_fn(int x) { return x + *a_ptr + *b_ptr + fps[0](x); }
C
cat > $t/b.c <<'C'
int b_val = 4;
static int hidden_counter;
int b_fn(int x) { return x * 2 + hidden_counter++; }
int b_unused(int x) { return x; }
C
for f in a b; do "${GNU}gcc" -O1 -marm -fno-pic -mword-relocations -fno-common -c -o $t/$f.o $t/$f.c && "${GNU}gcc" -O1 -marm -fPIC -fno-common -c -o $t/$f.pic.o $t/$f.c || exit 2; done
dynsyms() { "${GNU}readelf" --dyn-syms -W "$1" | awk '$1 ~ /^[0-9]+:$/ {print $4, $5, ($7 == "UND" ? "UND" : "DEF"), $8}' | sort; }
nbucket() { "${GNU}objcopy" -O binary -j .hash "$1" /dev/stdout | od -An -tu4 -N4 | tr -d ' '; }
rels() { "${GNU}readelf" -rW "$1" | awk '/^Relocation section/ {s=$3} $3 ~ /^R_ARM/ {print s, $3, $5}' | sort | uniq -c; }   # (our default layout isn't GNU's: no addresses)
dyntags() { "${GNU}readelf" -dW "$1" | awk '$1 ~ /^0x/ {print $2}' | tr -d '()' | grep -v '^NULL$' | sort | tr '\n' ' '; }
check() {   # name, link args...
  local n=$1; shift
  "${GNU}ld" --no-warn-rwx-segments "$@" -o $t/g.out 2>$t/g.err || { no "$n: GNU ld failed: $(head -1 $t/g.err)"; return; }
  "$LD" --no-warn-rwx-segments "$@" -o $t/o.out 2>$t/o.err || { no "$n: our ld failed: $(head -1 $t/o.err)"; return; }
  if diff <(dynsyms $t/g.out | grep -v ' UND ' | awk '$1 == "SECTION" || $3 == "DEF" {print $1, $2, $3}' | sort | uniq -c) \
          <(dynsyms $t/o.out | grep -v ' UND ' | awk '$1 == "SECTION" || $3 == "DEF" {print $1, $2, $3}' | sort | uniq -c) >$t/d \
     && [ "$("${GNU}readelf" --dyn-syms -W $t/o.out | awk '$1 == "0:" {print $2, $3, $7}')" = "00000000 0 UND" ]; then ok
  else no "$n: .dynsym's shape differs from GNU's (< GNU, > ours):"; sed 's/^/    /' $t/d | head -6; fi
  [ "$(nbucket $t/g.out)" = "$(nbucket $t/o.out)" ] && ok || no "$n: .hash nbucket $(nbucket $t/o.out), GNU's $(nbucket $t/g.out)"
  if diff <(rels $t/g.out) <(rels $t/o.out) >$t/d; then ok; else no "$n: dynamic relocations differ (< GNU, > ours):"; sed 's/^/    /' $t/d | head -8; fi
}
check "pie combreloc" -pie -e a_fn $t/a.o $t/b.o
check "pie nocombreloc" -pie -z nocombreloc -e a_fn $t/a.o $t/b.o
check "shared" -shared $t/a.pic.o $t/b.pic.o
check "shared -Bsymbolic" -shared -Bsymbolic $t/a.pic.o $t/b.pic.o
"$LD" --no-warn-rwx-segments -pie -e a_fn $t/a.o $t/b.o -o $t/p.out && "${GNU}ld" --no-warn-rwx-segments -pie -e a_fn $t/a.o $t/b.o -o $t/gp.out
for tag in TEXTREL RELCOUNT; do
  case "$(dyntags $t/gp.out)" in *"$tag "*) case "$(dyntags $t/p.out)" in *"$tag "*) ok;; *) no "pie: no DT_$tag (GNU has one)";; esac;; esac
done
[ "$("${GNU}readelf" -dW $t/p.out | awk '/RELCOUNT/ {print $3}')" = "$("${GNU}readelf" -dW $t/gp.out | awk '/RELCOUNT/ {print $3}')" ] && ok || no "pie: DT_RELCOUNT differs from GNU's"
"${GNU}readelf" -lW $t/p.out | grep -q INTERP && ok || no "pie: no PT_INTERP (GNU names one)"
"$LD" --no-warn-rwx-segments -pie --no-dynamic-linker -e a_fn $t/a.o $t/b.o -o $t/n.out && ! "${GNU}readelf" -lW $t/n.out | grep -q INTERP && ok || no "--no-dynamic-linker: a PT_INTERP"
"$LD" --no-warn-rwx-segments -pie -z text -e a_fn $t/a.o $t/b.o -o $t/x.out 2>/dev/null && no "-z text: a text relocation was accepted" || ok
# U-Boot's shape: -pie --no-dynamic-linker, every dynamic table but .hash discarded, .hash (no exports) in the image
cat > $t/u.ld <<'L'
SECTIONS { . = 0; .text : { *(.text*) } .rodata : { *(.rodata*) } .data : { *(.data*) } .rel.dyn : { *(.rel*) }
  /DISCARD/ : { *(.dynsym) *(.dynstr*) *(.dynamic*) *(.gnu*) *(.interp*) } }
L
"${GNU}ld" --no-warn-rwx-segments -pie --no-dynamic-linker -T $t/u.ld -e a_fn $t/a.o $t/b.o -o $t/gu.out 2>/dev/null
"$LD" --no-warn-rwx-segments -pie --no-dynamic-linker -T $t/u.ld -e a_fn $t/a.o $t/b.o -o $t/ou.out 2>$t/e
for s in .hash .rel.dyn; do
  cmp -s <("${GNU}objcopy" -O binary -j $s $t/gu.out /dev/stdout) <("${GNU}objcopy" -O binary -j $s $t/ou.out /dev/stdout) && ok || no "U-Boot-style pie: $s not byte-identical to GNU's ($(head -1 $t/e))"
done
echo "ld dynamic tables: $pass passed, $fail failed"
[ $fail -eq 0 ]
