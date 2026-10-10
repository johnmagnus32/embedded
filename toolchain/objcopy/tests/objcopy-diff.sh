#!/usr/bin/env bash
# objcopy-diff.sh — OUR objcopy/strip vs GNU's, byte for byte (the output file, its mode, the exit status), over the
# option sets real builds use: the kernel (Image/zImage: -O binary -R .comment -S; modules.builtin.modinfo: -j .modinfo
# -O binary of vmlinux.o), BusyBox (strip -s -R .note -R .comment), plus -g/--strip-unneeded/-x/-X/-K/-N/-R/-j,
# --gap-fill/--pad-to, in-place rewrites. Inputs: objects/executables/shared objects GNU built and ones OURS built.
# Env: GNU=<cross prefix>, OBJCOPY=<our objcopy>, STRIP=<our strip>.
set -u
here="$(cd "$(dirname "$0")" && pwd)"; img="$here/../../../projects/gameboy-v3/image/build/qemu"; tc="$here/../.."
S="$img/libc/stage-musl-static"
t=$(mktemp -d); trap 'rm -rf $t' EXIT
pass=0; fail=0
mkdir -p $t/g $t/o
cat > $t/p.c <<'C'
static int counter; int shared = 3; extern int printf(const char *, ...);
static int helper(int x) { return x * 2 + counter++; }
int api(int v) { return helper(v) + shared; }
int main(void) { printf("%d\n", api(4)); return 0; }
C
"${GNU}gcc" -O1 -g -c -o $t/p.o $t/p.c
"${GNU}gcc" -O1 -g -ffunction-sections -fdata-sections -c -o $t/pfs.o $t/p.c
objs=($t/p.o $t/pfs.o)
execs=()
b=$t/bin; mkdir -p $b $t/lib                    # our toolchain, in the driver's installed layout
cp "$tc/os-cc" $b/os-gcc; ln -s "$tc/cpp/build/cpp" $b/os-cpp; ln -s "$tc/cc/build/cc" $b/os-cc1
ln -s "$tc/as/build/as" $b/os-as; ln -s "$tc/ld/build/ld" $b/os-ld; ln -s "$tc/rt/build/libosrt.a" $t/lib/libosrt.a
if [ -d "$S" ]; then
  "${GNU}gcc" --sysroot="$S" -static -O1 -g -o $t/p.gnu $t/p.c && execs+=($t/p.gnu)
  $b/os-gcc --sysroot="$S" -static -o $t/p.ours $t/p.c && execs+=($t/p.ours) || echo "FAIL: our toolchain didn't link p.c"   # by OUR ld
  $b/os-gcc -c -o $t/p.ourobj.o $t/p.c && objs+=($t/p.ourobj.o) || echo "FAIL: our toolchain didn't compile p.c"            # by OUR as
fi
for f in "$img/linux/init/main.o" "$img/linux/kernel/sched/core.o" "$S/usr/lib/crt1.o" "$S/usr/lib/crti.o"; do [ -f "$f" ] && objs+=("$f"); done
for f in $(find "$img/libc" -maxdepth 3 -name '*.so*' -type f 2>/dev/null | head -2); do execs+=("$f"); done
[ -f "$img/linux/vmlinux" ] && execs+=("$img/linux/vmlinux")

# run <name> <gnu-tool> <our-tool> <args>, where @NAME is a file in each tool's own directory: @in is the input, a
# fresh copy per side, and any other @NAME the output. Both tools are run from their own directory so that a path
# INSIDE the output (an S0 header carries the output's name) is the same on both sides.
run() {
  local name="$1" gt="$2" ot="$3" in="$4"; shift 4
  cp "$in" $t/g/in; cp "$in" $t/o/in; chmod 640 $t/g/in $t/o/in
  local args=() o_name=
  for a in "$@"; do case "$a" in @in) ;; @*) o_name="${a#@}";; esac; args+=("${a//@/}"); done
  [ -z "$o_name" ] || rm -f "$t/g/$o_name" "$t/o/$o_name"
  ( cd $t/g && "$gt" "${args[@]}" ) >$t/g.err 2>&1; local g=$?
  ( cd $t/o && "$ot" "${args[@]}" ) >$t/o.err 2>&1; local o=$?
  local res=$t/g/$o_name ores=$t/o/$o_name
  { [ -n "$o_name" ] && [ -f "$res" ]; } || { res=$t/g/in; ores=$t/o/in; }
  if [ $g = $o ] && cmp -s $res $ores && [ "$(stat -c %a $res)" = "$(stat -c %a $ores)" ]; then pass=$((pass+1)); else
    fail=$((fail+1)); echo "FAIL $name $(basename $in): exit gnu=$g ours=$o, modes $(stat -c %a $res) $(stat -c %a $ores)"
    [ $g = 0 ] || sed 's/^/    gnu: /' $t/g.err | head -2; [ $o = 0 ] || sed 's/^/    ours: /' $t/o.err | head -2
    cmp $res $ores 2>&1 | head -1 | sed 's/^/    /'; fi
}
oc() { run "objcopy $*" "${GNU}objcopy" "$OBJCOPY" "$@"; }
st() { run "strip $*" "${GNU}strip" "$STRIP" "$@"; }
for f in "${objs[@]}" "${execs[@]}"; do
  oc "$f" @in @out;  oc "$f" -g @in @out;  oc "$f" --strip-unneeded @in @out;  oc "$f" -R .comment @in @out
  oc "$f" -x @in @out;  oc "$f" -X @in @out;  oc "$f" -g @in;  oc "$f" -K main -K api --strip-unneeded @in @out
  st "$f" -g @in;  st "$f" --strip-unneeded @in -o @out;  st "$f" -g -o @out @in
done
for f in "${objs[@]}"; do
  oc "$f" -j .text @in @out; oc "$f" -O binary -j .text @in @out; oc "$f" -N helper @in @out
  oc "$f" --remove-section=.note.gnu.property --rename-section .data=.data.efistub --rename-section .bss=.bss.efistub,load,alloc @in @out   # the EFI stub's
  for fl in alloc readonly alloc,code contents,alloc,load noload alloc,load,readonly,data debug contents merge,strings; do
    oc "$f" --rename-section .data=.d2,$fl --rename-section .text=.t2 @in @out; oc "$f" --set-section-flags .bss=$fl @in @out
  done
done
for f in "${execs[@]}"; do
  oc "$f" -O binary -R .comment -S @in @out;  oc "$f" -S @in @out;  st "$f" @in;  st "$f" -s -R .note -R .comment @in -o @out
  oc "$f" -O binary --gap-fill=0xff @in @out;  oc "$f" -O binary --pad-to=0x400000 --gap-fill=0x5a @in @out
  # the TEXT formats: the record type follows the address width, holes between sections are skipped, and
  # --gap-fill/--pad-to materialise them (so the same input is covered with and without)
  oc "$f" -O srec @in @out;  oc "$f" -O ihex @in @out
  oc "$f" -O srec -R .comment -S @in @out;  oc "$f" -O ihex -j .text @in @out
done
# The TEXT formats' address handling, which has a rule per range. S-records: ONE type for the whole file, from the
# widest address in it (S1/S9 under 64K, S2/S8 under 16M, else S3/S7) — so a low image whose sections are far apart
# is all S2, and --pad-to past 64K promotes the type. Intel HEX: a type-02 segment base inside the first megabyte,
# a type-04 linear base above it (clearing the segment base first), no record across a 64K boundary, and an entry
# address as type-03 or type-05 — or, when it is zero, not at all.
if [ -n "${GNU}" ] && [ -x "${GNU}gcc" ]; then
  printf 'int g=5;\nint f(int x){return x+g;}\n' > "$t/w.c"
  "${GNU}gcc" -O1 -marm -c -o "$t/w.o" "$t/w.c" 2>/dev/null
  link() {   # link <name> <ld args...>: one tiny image, then every text-format case over it
    local n="$1"; shift
    "${GNU}ld" "$@" -o "$t/w.$n" "$t/w.o" 2>/dev/null || return 0
    oc "$t/w.$n" -O srec @in @out;             oc "$t/w.$n" -O ihex @in @out
    oc "$t/w.$n" -O srec --gap-fill=0xaa @in @out;  oc "$t/w.$n" -O ihex --gap-fill=0xaa @in @out
    oc "$t/w.$n" -O srec --pad-to=0x20000 --gap-fill=0 @in @out
    oc "$t/w.$n" -O ihex --pad-to=0x20000 --gap-fill=0 @in @out
  }
  spread() { cat > "$t/w.ld" <<L
ENTRY(f)
SECTIONS { . = $1; .text : { *(.text) } . = $2; .data : { *(.data) } }
L
  }
  for base in 0x8000 0x200000 0x40000000; do spread "${base}" "${base} + 0x9000"; link "$base" -T "$t/w.ld"; done
  spread 0x100 .;      link low   -T "$t/w.ld"             # all under 64K: S1/S9, and no base record at all
  spread 0x100 .;      link zero  -T "$t/w.ld" -e 0        # entry 0: an S9 carrying 0, and NO ihex start record
  spread 0x8000 0x200000;  link mix  -T "$t/w.ld"          # page 0 then a linear base
  spread 0x11000 0x200000; link seg2ext -T "$t/w.ld"       # a segment base, then a linear one over it
  spread 0xfff8 .;     link cross -T "$t/w.ld"             # a section straddling a 64K boundary
  # the S0 header carries the OUTPUT NAME as given, 40 bytes at most
  long=aaaaaaaaaa/bbbbbbbbbb/cccccccccc/dddddddddd/eeeeeeeeee
  for d in $t/g $t/o; do mkdir -p "$d/$long"; done
  oc "$t/w.low" -O srec @in @$long/ffff.srec
fi
[ -f "$img/linux/vmlinux.o" ] && oc "$img/linux/vmlinux.o" -j .modinfo -O binary @in @out
[ -f "$img/linux/arch/arm/boot/compressed/vmlinux" ] && oc "$img/linux/arch/arm/boot/compressed/vmlinux" -O binary -R .comment -S @in @out
echo "objcopy-diff: $pass identical, $fail different over ${#objs[@]} objects + ${#execs[@]} executables/shared objects"
[ $fail = 0 ]
