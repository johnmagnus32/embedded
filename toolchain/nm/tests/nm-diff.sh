#!/usr/bin/env bash
# nm-diff.sh — OUR nm vs GNU nm, byte for byte (stdout + stderr + exit status), across option sets and inputs:
# the class probe, plus (when the image build is present) kernel objects/archives/vmlinux, BusyBox, musl's libc.a,
# and shared objects (-D). Env: GNU=<cross prefix>, NM=<our nm>.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; img="$here/../../../projects/gameboy-v3/image/build/qemu"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
"${GNU}as" -o $t/classes.o "$here/classes.s" || { echo "FAIL: GNU as on classes.s"; exit 1; }
"${GNU}ar" rc $t/lib.a $t/classes.o; "${GNU}ar" rcT $t/thin.a $t/classes.o
inputs=($t/classes.o $t/lib.a $t/thin.a)
for f in "$img/linux/vmlinux" "$img/linux/vmlinux.o" "$img/linux/init/main.o" "$img/linux/kernel/sched/core.o" "$img/linux/lib/lib.a" \
         "$img/linux/arch/arm/vdso/vdso.so.raw" "$img/libc/stage-musl-static/usr/lib/libc.a" "$img/libc/stage-musl-static/usr/lib/crt1.o"; do
  [ -f "$f" ] && inputs+=("$f")
done
dyn=(); for f in $(find "$img/libc" -maxdepth 3 -name '*.so*' -type f 2>/dev/null | head -2); do dyn+=("$f"); done
pass=0; fail=0
check() {   # check <args...>
  "${GNU}nm" "$@" >$t/g.out 2>$t/g.err; g=$?; "$NM" "$@" >$t/o.out 2>$t/o.err; o=$?
  sed -i 's/^[^:]*nm: /nm: /' $t/g.err $t/o.err
  if [ $g = $o ] && cmp -s $t/g.out $t/o.out && cmp -s $t/g.err $t/o.err; then pass=$((pass+1)); else
    fail=$((fail+1)); echo "FAIL nm $*  (exit gnu=$g ours=$o)"; diff $t/g.out $t/o.out | head -5; diff $t/g.err $t/o.err | head -3; fi
}
for f in "${inputs[@]}"; do
  for o in "" -n -S -a -g -u -U -p -r -A -nr -gS --size-sort --special-syms "-n -S" "-a -n"; do check $o "$f"; done
done
for f in "${dyn[@]}"; do for o in -D "-D -n" "-D -S" "-D -g"; do check $o "$f"; done; done
check "${inputs[0]}" "${inputs[1]}"                  # several files: a header per file
check -A "${inputs[0]}" "${inputs[1]}"
echo "nm-diff: $pass identical, $fail different over ${#inputs[@]} inputs + ${#dyn[@]} shared objects"
[ $fail = 0 ]
