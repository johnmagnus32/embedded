#!/usr/bin/env bash
# musl-selfbuild.sh — build REAL musl with OUR WHOLE TOOLCHAIN and run programs against it. The strongest
# interchangeability check there is: musl 1.2.5's own build system, its own configure, its ~1420 sources —
# preprocessed by our cpp, compiled by our cc, assembled by our as, archived by our ar, linked by our ld, with our
# runtime — then booted as PID 1 on a real Linux kernel. Anything our toolchain gets wrong about C, the ABI, the
# object format or the archive format shows up as a miscompile here, not as a warning.
#
#   MUSL_SRC=<musl source tree>  TC=<toolchain-custom bin>  SYSROOT_INC=<musl headers>
#   REFKERNEL=<zImage>  GEN_INIT_CPIO=<tool>  [WORK=<dir>]  musl-selfbuild.sh
#
# The programs are the same ones musl-link.sh boots against the GCC-built libc, so a failure here against a pass
# there localizes the bug to our toolchain rather than to the program or the libc.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); T=$(cd "$HERE/.." && pwd)
: "${MUSL_SRC:?}" "${TC:?}" "${SYSROOT_INC:?}" "${REFKERNEL:?}" "${GEN_INIT_CPIO:?}"
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
CC="$TC/arm-os-custom-gcc"; AR="$TC/arm-os-custom-ar"
for t in "$CC" "$AR" "$TC/os-cpp" "$TC/os-cc1" "$TC/os-as" "$TC/os-ld"; do [ -x "$t" ] || { echo "SKIP musl self-build: $t missing"; exit 0; }; done

# ---- configure + build musl with our tools (out of tree; musl's own build system, unpatched) -------------------
B="$W/build"; mkdir -p "$B"
if [ ! -f "$B/config.mak" ]; then
  ( cd "$B" && "$MUSL_SRC/configure" --host=arm-os-custom-linux-gnueabihf --prefix=/usr --disable-shared \
      CC="$CC" CFLAGS="-mcpu=cortex-a7 -marm -O2" ) > "$B/configure.log" 2>&1 \
    || { echo "FAIL musl self-build: configure"; tail -15 "$B/configure.log"; exit 1; }
fi
# RANLIB=true: our ar always writes the archive index; AR/RANLIB are named for the --host triple our bin doesn't use
if ! ( cd "$B" && make -j"$(nproc)" lib/libc.a lib/crt1.o lib/crti.o lib/crtn.o AR="$AR" RANLIB=true ) > "$B/make.log" 2>&1; then
  echo "FAIL musl self-build: build"; grep -E '^(cpp|cc|as|ar|ld|make)(:|\[)' "$B/make.log" | grep -v '^make\[' | head -8; exit 1
fi
n=$(cd "$B" && find obj -name '*.o' | wc -l)
echo "PASS musl self-build: $n objects + libc.a + crt1/crti/crtn, all by our cpp/cc/as/ar"

# ---- run programs against it ----------------------------------------------------------------------------------
L="$B/lib"
run() {   # run NAME WANT -- src...
  local name=$1 want=$2; shift 3
  local objs=() i=0
  for f in "$@"; do
    "$CC" -std=c99 -nostdinc -isystem "$SYSROOT_INC" -O2 -c -o "$W/o$i.o" "$f" || { echo "FAIL $name: compile $f"; exit 1; }
    objs+=("$W/o$i.o"); i=$((i + 1))
  done
  "$T/ld/build/ld" -o "$W/init" "$L/crt1.o" "$L/crti.o" "${objs[@]}" "$L/libc.a" "$T/rt/build/libosrt.a" "$L/crtn.o" \
    || { echo "FAIL $name: link"; exit 1; }
  { echo 'dir /dev 0755 0 0'; echo 'nod /dev/console 0600 0 0 c 5 1'; echo "file /init $W/init 0755 0 0"; } > "$W/list"
  "$GEN_INIT_CPIO" "$W/list" | gzip -9 > "$W/initrd"
  timeout 90 qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none -kernel "$REFKERNEL" \
    -initrd "$W/initrd" -append "console=ttyAMA0 rdinit=/init panic=1" > "$W/log" 2>&1 || true
  grep -qF -- "$want" "$W/log" || { echo "FAIL $name: '$want' missing — last lines:"; tail -8 "$W/log" | sed 's/^/    /'; exit 1; }
  grep -qF 'exitcode=0x00000000' "$W/log" || { echo "FAIL $name: init did not exit 0"; tail -8 "$W/log" | sed 's/^/    /'; exit 1; }
  echo "PASS $name: '$want'"
}
run "musl self-built: stdio/printf/errno" "musl-link 13579 142857142874 7.500 erange" -- "$HERE/musl-link.c"
run "musl self-built: TLS + threads" "musl-tls 1 m 9 154 70 50" -- "$HERE/musl-tls-cc.c" "$HERE/musl-tls-cc2.c"
run "musl self-built: trampolines" "musl-tramp 120091 970011 131206 23" -- "$HERE/musl-tramp-cc.c"
run "musl self-built: math (its VFP-asm sqrt/fabs/fma)" \
    "musl-math 1.414214 1.500000 17.000000 1024.000000 0.693147 1.41421 3.50000 7.00000" -- "$HERE/musl-math.c"
[ -n "${WORK:-}" ] || rm -rf "$W"
