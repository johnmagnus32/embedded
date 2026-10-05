#!/usr/bin/env bash
# ar-diff.sh — OUR ar vs GNU ar, byte for byte (the archive, stdout, the exit status) across the operations builds use:
# rc / rcs / rcS / cru / q / d / m (a, b, i) / s / t / p / x, D and U, P, and the kernel's thin archives (cDPrST of
# nested thin archives, then mPiT and t). In U mode the index's date (the time it was written) is masked.
# Env: GNU=<cross prefix>, AR=<our ar>.
set -u
t=$(mktemp -d); trap 'rm -rf $t' EXIT
mkdir -p $t/w/d/e $t/g $t/o; cd $t/w
mk() { printf "$2" | "${GNU}as" -o "$1"; }
mk d/a.o '.global a1\na1: nop\n.weak w1\nw1: nop\n'; mk d/e/b_long_name_object.o '.global b1\nb1: nop\n.comm cm,4\n'
mk c.o 'c: nop\n'; mk sixteen_chars.o '.global s1\ns1: bx lr\n'; mk fifteen_chr.o '.global f1\nf1: nop\n'; printf 'not an object\n' > notes.txt
touch -d '2020-01-02 03:04:05' d/a.o c.o
pass=0; fail=0
mask() { python3 - "$1" <<'P'
import sys
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
if d[8:10] == b'/ ': d[24:36] = b'X' * 12          # the index member's date: when it was written
open(p, 'wb').write(d)
P
}
case_() {   # case_ <name> <setup: shell run in each dir before> <ar args...>   (archive: x.a; outputs compared)
  local name="$1" setup="$2"; shift 2
  for side in g o; do
    rm -rf $t/$side; cp -a $t/w $t/$side; ( cd $t/$side && eval "$setup" )
    local tool; [ $side = g ] && tool="${GNU}ar" || tool="$AR"
    ( cd $t/$side && "$tool" "$@" >$t/$side.out 2>$t/$side.err; echo $? > $t/$side.rc )
    [ -f $t/$side/x.a ] && mask $t/$side/x.a
  done
  if cmp -s $t/g.rc $t/o.rc && cmp -s $t/g.out $t/o.out && { [ ! -f $t/g/x.a ] || cmp -s $t/g/x.a $t/o/x.a; } \
     && diff -r $t/g $t/o >/dev/null; then pass=$((pass+1)); else
    fail=$((fail+1)); echo "FAIL $name: ar $*  (rc gnu=$(cat $t/g.rc) ours=$(cat $t/o.rc))"; sed 's/^/    gnu: /' $t/g.err | head -2; sed 's/^/    ours: /' $t/o.err | head -2
    [ -f $t/g/x.a ] && cmp $t/g/x.a $t/o/x.a 2>&1 | head -1 | sed 's/^/    /'; diff -r $t/g $t/o 2>&1 | head -2 | sed 's/^/    /'; fi
}
objs="d/a.o c.o d/e/b_long_name_object.o sixteen_chars.o fifteen_chr.o"
for m in D U; do
  case_ "rc$m"   ""                                   rc$m x.a $objs
  case_ "rcs$m"  ""                                   rcs$m x.a $objs notes.txt
  case_ "rcS$m"  ""                                   rcS$m x.a $objs
  case_ "rcP$m"  ""                                   rcP$m x.a $objs
  case_ "cru$m"  "${AR_FOR_SETUP:-true}"               cru$m x.a $objs
  case_ "q$m"    ""                                   q$m x.a $objs
  case_ "r-existing$m" "\"\$AR_SETUP\" rc$m x.a d/a.o c.o"   r$m x.a fifteen_chr.o c.o
  case_ "d$m"    "\"\$AR_SETUP\" rc$m x.a $objs"         d$m x.a c.o fifteen_chr.o
  case_ "m$m"    "\"\$AR_SETUP\" rc$m x.a $objs"         m$m x.a c.o
  case_ "mb$m"   "\"\$AR_SETUP\" rc$m x.a $objs"         mb$m d/e/b_long_name_object.o x.a fifteen_chr.o sixteen_chars.o
  case_ "ma$m"   "\"\$AR_SETUP\" rc$m x.a $objs"         ma$m a.o x.a fifteen_chr.o
  case_ "s$m"    "\"\$AR_SETUP\" rcS$m x.a $objs"        s$m x.a
  case_ "thin$m" ""                                   cDPrST x.a $objs
  case_ "thin-nested$m" "\"\$AR_SETUP\" cDPrST d/sub.a d/a.o d/e/b_long_name_object.o"   cDPrST x.a c.o d/sub.a sixteen_chars.o
  case_ "thin-mPiT$m" "\"\$AR_SETUP\" cDPrST x.a c.o d/a.o d/e/b_long_name_object.o"      mPiT c.o x.a d/e/b_long_name_object.o d/a.o
  case_ "thin-index$m" ""                             cDPrT x.a $objs
done
case_ t    "\"\$AR_SETUP\" rcD x.a $objs"   t x.a
case_ tP   "\"\$AR_SETUP\" rcPD x.a $objs"  tP x.a
case_ tthin "\"\$AR_SETUP\" cDPrST x.a $objs" t x.a
case_ p    "\"\$AR_SETUP\" rcD x.a $objs"   p x.a c.o
case_ x    "\"\$AR_SETUP\" rcD x.a $objs; rm c.o sixteen_chars.o"   x x.a c.o sixteen_chars.o
echo "ar-diff: $pass identical, $fail different"
[ $fail = 0 ]
