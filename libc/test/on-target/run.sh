#!/usr/bin/env bash
# run.sh — on-target unit tests for the libc's SYSCALL WRAPPERS (need a real kernel).
#
# Syscall-wrapper behavior (e.g. WNOHANG) can't be unit-tested on the host — only a real
# kernel exercises it. So each test program is linked STATICALLY against the custom libc
# and booted as PID 1 on a mainline reference kernel under QEMU; we grep its console for a
# PASS marker. Add cases here as socket/epoll/signalfd/timerfd/mmap wrappers land.
#
# This harness is SELF-CONTAINED: it knows nothing about the os/ build system. It takes the
# libc-under-test through the cross toolchain's baked sysroot, the kernel + host tools as
# paths, and boots them. Whoever drives it (the product's `make test`, a CI job, or you by
# hand) is responsible for building those artifacts and passing them in.
#
# ---- INPUT CONTRACT (environment) -------------------------------------------
#   CROSS_COMPILE   (required) cross-gcc prefix whose BAKED SYSROOT is the custom libc under
#                   test, e.g. /path/to/arm-none-linux-gnueabihf- . A normal static link
#                   (-static) against this toolchain pulls in that sysroot's libc + crt.
#   KERNEL          (required) path to a bootable QEMU `virt` zImage (mainline ARM, honors
#                   WNOHANG etc.). Booted with rdinit=/init, panic=1.
#   GEN_INIT_CPIO   (required) path to a gen_init_cpio host binary (packs the initramfs).
#   QEMU            (optional) qemu-system-arm binary. Default: qemu-system-arm.
#
# The `-M virt -cpu cortex-a7 -marm -mgeneral-regs-only` flags are intrinsic to a QEMU virt
# ARM test and are hardcoded on purpose (same as the boot line below).
#
# Exit 0 iff every case that ran PASSED.
set -u

: "${CROSS_COMPILE:?run.sh: set CROSS_COMPILE=<cross-gcc prefix whose sysroot is the libc under test>}"
: "${KERNEL:?run.sh: set KERNEL=<path to a bootable virt zImage>}"
: "${GEN_INIT_CPIO:?run.sh: set GEN_INIT_CPIO=<path to a gen_init_cpio binary>}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOGDIR="${LOGDIR:-${HERE}/logs}"; mkdir -p "${LOGDIR}"
QEMU="${QEMU:-qemu-system-arm}"
ARCH_FLAGS="-mcpu=cortex-a7 -marm -mgeneral-regs-only"   # QEMU virt ARM, VFP-free

red(){ printf '\033[31m%s\033[0m\n' "$*"; }; grn(){ printf '\033[32m%s\033[0m\n' "$*"; }
die(){ red "ERROR: $*"; exit 1; }

# ---- prerequisites --------------------------------------------------------
command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 || die "cross gcc not found: ${CROSS_COMPILE}gcc"
[ -f "${KERNEL}" ]        || die "KERNEL not found: ${KERNEL}"
[ -x "${GEN_INIT_CPIO}" ] || die "GEN_INIT_CPIO not executable: ${GEN_INIT_CPIO}"
command -v "${QEMU}" >/dev/null 2>&1 || die "${QEMU} not on PATH"

ran=0; fails=0
run_prog() {   # <name.c> <marker>
	local src="${HERE}/$1" name="${1%.c}" marker="$2"
	local dir; dir="$(mktemp -d)"
	printf '\n=== case: %s ===\n' "$name"
	ran=$((ran + 1))
	# NORMAL static link against the toolchain's own sysroot (which IS the custom libc under
	# test) — the toolchain supplies crt + libc + libgcc. No bare-metal link model.
	if ! "${CROSS_COMPILE}gcc" ${ARCH_FLAGS} -static -Wall -Wextra \
	     -o "${dir}/init" "${src}" 2>"${dir}/cc.log"; then
		red "  FAIL  (link)"; sed 's/^/    /' "${dir}/cc.log"; fails=$((fails + 1)); rm -rf "${dir}"; return
	fi
	{ echo 'dir /dev 0755 0 0'; echo 'nod /dev/console 0600 0 0 c 5 1'
	  printf 'file /init %s/init 0755 0 0\n' "${dir}"; } > "${dir}/list"
	"${GEN_INIT_CPIO}" "${dir}/list" | gzip -9 > "${dir}/initrd.cpio.gz"
	local log="${LOGDIR}/unit-${name}.log"
	timeout 60 "${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
		-kernel "${KERNEL}" -initrd "${dir}/initrd.cpio.gz" \
		-append "console=ttyAMA0 rdinit=/init panic=1" > "${log}" 2>&1
	if grep -qF -- "${marker}" "${log}"; then
		grn "  PASS  (saw '${marker}')  log: ${log}"
	else
		red "  FAIL  (marker '${marker}' not found)  log: ${log}"; tail -8 "${log}" | sed 's/^/    /'
		fails=$((fails + 1))
	fi
	rm -rf "${dir}"
}

run_prog waittest.c "WNOHANG_OK"

echo
if [ "${fails}" -eq 0 ]; then grn "ON-TARGET UNITS: ${ran}/${ran} PASS"; else red "ON-TARGET UNITS: $((ran - fails))/${ran} pass"; exit 1; fi
