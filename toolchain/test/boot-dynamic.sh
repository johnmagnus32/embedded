#!/usr/bin/env bash
# boot-dynamic.sh — end-to-end test of DYNAMIC linking with the from-scratch toolchain. It compiles a
# program with our OWN cpp/cc/as (-fPIC), links it against our OWN libc.so with our ld (-lc -L), producing
# an ET_EXEC that names /lib/ld.so.1 as its PT_INTERP and libc.so.1 as its DT_NEEDED, packs it + our
# ld.so.1 + libc.so.1 into an initramfs, and boots it as PID 1 on a mainline ARM kernel under QEMU -M virt.
# PASS = the program's computed marker reaches the console — i.e. the WHOLE dynamic chain works:
#   kernel maps ld.so.1 -> ld.so.1 self-relocates -> maps libc.so -> resolves the program's PLT/GOT
#   (printf/__libc_start_main against libc) + libc's PLT (main against the program) -> jumps to the program.
#
# Sibling of the static boot.sh; same "self-contained component test" discipline (knows nothing about the
# os/ engine — whoever drives it passes the staged artifacts as paths).
#
# ---- INPUT CONTRACT (environment) -------------------------------------------
#   CC             (required) our gcc-shaped driver, e.g. .../bin/arm-os-custom-gcc
#   LIBC_DYN_STAGE (required) dir holding crt0.S.o + libc.so + ld.so.1 (the DYNAMIC libc stage)
#   LIBC_INCLUDE   (required) our libc headers dir (-I)
#   UAPI_INCLUDE   (required) staged kernel UAPI headers dir (-I)
#   GEN_INIT_CPIO  (required) gen_init_cpio host binary (packs the initramfs)
#   REFKERNEL      (required) bootable mainline QEMU virt zImage
#   QEMU           (optional) qemu-system-arm binary (default: qemu-system-arm)
#
# Exit 0 iff the dynamically-linked program booted and printed its marker.
set -u
: "${CC:?boot-dynamic.sh: set CC=<os-cc driver>}"
: "${LIBC_DYN_STAGE:?boot-dynamic.sh: set LIBC_DYN_STAGE=<dir with crt0.S.o + libc.so + ld.so.1>}"
: "${LIBC_INCLUDE:?boot-dynamic.sh: set LIBC_INCLUDE=<our libc headers dir>}"
: "${UAPI_INCLUDE:?boot-dynamic.sh: set UAPI_INCLUDE=<staged kernel UAPI headers dir>}"
: "${GEN_INIT_CPIO:?boot-dynamic.sh: set GEN_INIT_CPIO=<gen_init_cpio binary>}"
: "${REFKERNEL:?boot-dynamic.sh: set REFKERNEL=<bootable mainline virt zImage>}"
QEMU="${QEMU:-qemu-system-arm}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${HERE}/boot/init.c"                 # reuse the static test's init: printf + a computed marker
BUILD="${BUILD:-${HERE}/.bootbed-dyn}"
MARKER="add(2, 3) = 5"

red(){ printf '\033[31m%s\033[0m\n' "$*"; }
grn(){ printf '\033[32m%s\033[0m\n' "$*"; }
die(){ red "boot-dynamic.sh ERROR: $*"; exit 1; }

command -v "${CC}" >/dev/null 2>&1 || die "CC not found: ${CC}"
[ -f "${LIBC_DYN_STAGE}/crt0.S.o" ] || die "no crt0.S.o in ${LIBC_DYN_STAGE}"
[ -f "${LIBC_DYN_STAGE}/libc.so" ]  || die "no libc.so in ${LIBC_DYN_STAGE}"
[ -f "${LIBC_DYN_STAGE}/ld.so.1" ]  || die "no ld.so.1 in ${LIBC_DYN_STAGE}"
[ -f "${REFKERNEL}" ]               || die "REFKERNEL not found: ${REFKERNEL}"
[ -x "${GEN_INIT_CPIO}" ]           || die "GEN_INIT_CPIO not executable: ${GEN_INIT_CPIO}"
command -v "${QEMU}" >/dev/null 2>&1 || die "${QEMU} not on PATH"

rm -rf "${BUILD}"; mkdir -p "${BUILD}"

echo "  cc + as : ${SRC##*/} -> init.o (-fPIC)"
"${CC}" -fPIC -I"${LIBC_INCLUDE}" -I"${UAPI_INCLUDE}" -c "${SRC}" -o "${BUILD}/init.o" || die "compile failed"
echo "  ld      : crt0 + init.o -lc -L stage -> init (dynamic ARM ELF, PT_INTERP=/lib/ld.so.1)"
"${CC}" -Ttext 0x40000000 -lc -L"${LIBC_DYN_STAGE}" "${LIBC_DYN_STAGE}/crt0.S.o" "${BUILD}/init.o" \
  -o "${BUILD}/init" || die "dynamic link failed"

echo "  pack    : /init + /lib/{ld.so.1,libc.so.1} + /dev/console -> initramfs.cpio.gz"
{ echo 'dir /dev 0755 0 0'
  echo 'nod /dev/console 0600 0 0 c 5 1'
  echo 'dir /lib 0755 0 0'
  echo "file /lib/ld.so.1 ${LIBC_DYN_STAGE}/ld.so.1 0755 0 0"
  echo "file /lib/libc.so.1 ${LIBC_DYN_STAGE}/libc.so 0755 0 0"
  echo "file /init ${BUILD}/init 0755 0 0"; } > "${BUILD}/init.list"
"${GEN_INIT_CPIO}" "${BUILD}/init.list" | gzip -9 > "${BUILD}/initramfs.cpio.gz"

echo "  boot    : ${QEMU} -M virt"
LOG="${BUILD}/boot.log"
timeout 60 "${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
  -kernel "${REFKERNEL}" -initrd "${BUILD}/initramfs.cpio.gz" \
  -append "console=ttyAMA0 rdinit=/init panic=1" > "${LOG}" 2>&1

if grep -qF -- "${MARKER}" "${LOG}"; then
  grn "PASS — dynamically-linked program booted via our ld.so.1 + libc.so (saw '${MARKER}')"
  exit 0
fi
red "FAIL — marker '${MARKER}' not found. Last lines of the boot log:"
tail -12 "${LOG}" | sed 's/^/    /'
exit 1
