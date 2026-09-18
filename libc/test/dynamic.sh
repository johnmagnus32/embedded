#!/usr/bin/env bash
# dynamic.sh — boot-test OUR from-scratch dynamic linker (ld.so.1).
#
# WHY THIS EXISTS: our dynamic linker (ld.so.1) is the most error-prone thing in
# the project. This harness boots a MAINLINE ARM Linux kernel (full dynamic-linking
# support) under QEMU `-M virt` with OUR dynamically-linked rootfs as init, so a
# failure is unambiguously OUR loader. PASS = ld.so.1 mapped /lib/libc.so, relocated,
# and handed off to an interactive /bin/sh (the 'gv3$' prompt is the marker).
#
# SELF-CONTAINED: it knows nothing about the os/ build system. It takes the reference
# kernel and our dynamic rootfs as PATHS; whoever drives it builds those and passes
# them in.
#
# ---- INPUT CONTRACT (environment) -------------------------------------------
#   REFKERNEL       (required) path to a bootable MAINLINE QEMU `virt` zImage (full
#                   dynamic-linking support).
#   DYNAMIC_INITRD  (required) path to OUR prebuilt dynamically-linked rootfs
#                   (PT_INTERP=/lib/ld.so.1).
#   QEMU            (optional) qemu-system-arm binary. Default: qemu-system-arm.
#
# Exit 0 iff the boot reached our shell.
set -u

: "${REFKERNEL:?dynamic.sh: set REFKERNEL=<path to a bootable mainline virt zImage>}"
: "${DYNAMIC_INITRD:?dynamic.sh: set DYNAMIC_INITRD=<path to our dynamically-linked rootfs cpio.gz>}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOGDIR="${LOGDIR:-${HERE}/logs}"
QEMU="${QEMU:-qemu-system-arm}"

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
ylw() { printf '\033[33m%s\033[0m\n' "$*"; }
info(){ printf '  %s\n' "$*"; }
die() { red "ERROR: $*"; exit 1; }

[ -f "${REFKERNEL}" ]      || die "REFKERNEL not found: ${REFKERNEL}"
[ -f "${DYNAMIC_INITRD}" ] || die "DYNAMIC_INITRD not found: ${DYNAMIC_INITRD}"
command -v "${QEMU}" >/dev/null 2>&1 || die "${QEMU} not on PATH"

mkdir -p "${LOGDIR}"
info "reference kernel: ${REFKERNEL} (given)"

# boot the reference kernel with an initramfs, check for a marker.
# run_case <name> <initrd> <required-marker>
run_case() {
  local name="$1" initrd="$2" marker="$3"
  local log="${LOGDIR}/dyn-${name}.log"
  printf '\n=== case: %s ===\n' "$name"
  # rdinit=/init runs our init as PID 1; panic=1 + -no-reboot makes a PID-1 exit
  # terminate QEMU promptly. -net none avoids QEMU's default virtio-net (a missing
  # efi-virtio.rom would abort it).
  timeout 60 "$QEMU" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
    -kernel "${REFKERNEL}" -initrd "${initrd}" \
    -append "console=ttyAMA0 rdinit=/init panic=1" >"${log}" 2>&1

  if grep -qF -- "${marker}" "${log}"; then
    grn "  PASS  (saw '${marker}')  log: ${log}"; return 0
  fi
  red "  FAIL  (marker '${marker}' not found)  log: ${log}"
  ylw "  --- last lines ---"; tail -6 "${log}" | sed 's/^/    /'
  return 1
}

# gv3 — OUR dynamic rootfs, driven by OUR ld.so.1. Boots the prebuilt DYNAMIC_INITRD;
# our init.sh is a shebang script, so the mainline kernel needs /bin/sh to be OUR
# dynamic shell, loaded by OUR ld.so.1. PASS = the loader mapped libc.so, relocated,
# and reached the interactive shell prompt ('gv3$').
if run_case gv3 "${DYNAMIC_INITRD}" "gv3\$"; then
  printf '\n'; grn "OK — dynamic-linking boot passed."
  exit 0
fi
printf '\n'; red "dynamic-linking boot FAILED."
exit 1
