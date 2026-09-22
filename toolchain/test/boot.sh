#!/usr/bin/env bash
# boot.sh — end-to-end test of the from-scratch toolchain. It compiles + assembles + links
# a program with our OWN cpp/cc/as/ld against our libc.a, packs it as /init into a minimal
# initramfs, and boots it as PID 1 on a mainline ARM kernel under QEMU -M virt. PASS = the
# program's computed marker ("add(2, 3) = 5") reaches the console — i.e. the whole chain
# (preprocess -> compile -> assemble -> link -> libc -> run) works.
#
# SELF-CONTAINED: it knows nothing about the os/ build system. Whoever drives it builds the
# custom toolchain + libc + a reference kernel and passes them in as PATHS (mirrors
# libc/test/dynamic.sh). So it stays a component test, decoupled from the engine.
#
# ---- INPUT CONTRACT (environment) -------------------------------------------
#   CC             (required) our gcc-shaped driver, e.g. .../bin/arm-os-custom-gcc
#   LIBC_STAGE     (required) dir holding crt0.S.o + libc.a (the static libc stage)
#   LIBC_INCLUDE   (required) our libc headers dir (-I)
#   UAPI_INCLUDE   (required) staged kernel UAPI headers dir (-I)
#   GEN_INIT_CPIO  (required) gen_init_cpio host binary (packs the initramfs)
#   REFKERNEL      (required) bootable mainline QEMU virt zImage
#   QEMU           (optional) qemu-system-arm binary (default: qemu-system-arm)
#
# Exit 0 iff the program booted and printed its marker.
set -u
: "${CC:?boot.sh: set CC=<os-cc driver>}"
: "${LIBC_STAGE:?boot.sh: set LIBC_STAGE=<dir with crt0.S.o + libc.a>}"
: "${LIBC_INCLUDE:?boot.sh: set LIBC_INCLUDE=<our libc headers dir>}"
: "${UAPI_INCLUDE:?boot.sh: set UAPI_INCLUDE=<staged kernel UAPI headers dir>}"
: "${GEN_INIT_CPIO:?boot.sh: set GEN_INIT_CPIO=<gen_init_cpio binary>}"
: "${REFKERNEL:?boot.sh: set REFKERNEL=<bootable mainline virt zImage>}"
QEMU="${QEMU:-qemu-system-arm}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${HERE}/boot/init.c"
BUILD="${BUILD:-${HERE}/.bootbed}"
MARKER="add(2, 3) = 5"

red(){ printf '\033[31m%s\033[0m\n' "$*"; }
grn(){ printf '\033[32m%s\033[0m\n' "$*"; }
die(){ red "boot.sh ERROR: $*"; exit 1; }

command -v "${CC}" >/dev/null 2>&1 || die "CC not found: ${CC}"
[ -f "${LIBC_STAGE}/crt0.S.o" ] || die "no crt0.S.o in ${LIBC_STAGE}"
[ -f "${LIBC_STAGE}/libc.a" ]   || die "no libc.a in ${LIBC_STAGE}"
[ -f "${REFKERNEL}" ]           || die "REFKERNEL not found: ${REFKERNEL}"
[ -x "${GEN_INIT_CPIO}" ]       || die "GEN_INIT_CPIO not executable: ${GEN_INIT_CPIO}"
command -v "${QEMU}" >/dev/null 2>&1 || die "${QEMU} not on PATH"

rm -rf "${BUILD}"; mkdir -p "${BUILD}"

echo "  cc + as : ${SRC##*/} -> init.o"
"${CC}" -I"${LIBC_INCLUDE}" -I"${UAPI_INCLUDE}" -c "${SRC}" -o "${BUILD}/init.o" || die "compile failed"
echo "  ld      : crt0 + init.o + libc.a -> init (static ARM ELF)"
"${CC}" "${LIBC_STAGE}/crt0.S.o" "${BUILD}/init.o" "${LIBC_STAGE}/libc.a" -Ttext 0x40000000 -o "${BUILD}/init" \
  || die "link failed (a libc symbol our cc still can't produce?)"

echo "  pack    : /init + /dev/console -> initramfs.cpio.gz"
{ echo 'dir /dev 0755 0 0'
  echo 'nod /dev/console 0600 0 0 c 5 1'
  echo "file /init ${BUILD}/init 0755 0 0"; } > "${BUILD}/init.list"
"${GEN_INIT_CPIO}" "${BUILD}/init.list" | gzip -9 > "${BUILD}/initramfs.cpio.gz"

echo "  boot    : ${QEMU} -M virt"
LOG="${BUILD}/boot.log"
timeout 60 "${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
  -kernel "${REFKERNEL}" -initrd "${BUILD}/initramfs.cpio.gz" \
  -append "console=ttyAMA0 rdinit=/init panic=1" > "${LOG}" 2>&1

if grep -qF -- "${MARKER}" "${LOG}"; then
  grn "PASS — our toolchain built a program that booted + ran (saw '${MARKER}')"
  exit 0
fi
red "FAIL — marker '${MARKER}' not found. Last lines of the boot log:"
tail -8 "${LOG}" | sed 's/^/    /'
exit 1
