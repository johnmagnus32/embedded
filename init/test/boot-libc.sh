#!/usr/bin/env bash
# init/test/boot-libc.sh — boot the CUSTOM init built against the CUSTOM libc as REAL PID 1
# on a MAINLINE kernel under QEMU. The capstone for "our libc runs our init on mainline".
#
# boot.sh proves the init against the OSS baseline (musl + busybox). This proves the init +
# the FROM-SCRATCH libc together: LIBC=custom INIT=custom, our coreutils, our /sbin/initctl —
# no musl, no busybox anywhere. The single oneshot drives `initctl poweroff`, which exercises
# the whole supervisor path end-to-end: early mounts, config parse (scandir/fopen/fgets),
# the epoll/signalfd/timerfd event loop, fork/execvp, the AF_UNIX control socket, and
# shutdown/reboot — all through the custom libc's syscall wrappers.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/../.." && pwd)"
PROJ="${GV3_PRODUCT:-${REPO}/projects/gameboy-v3}"
QEMU="${QEMU:-qemu-system-arm}"
LOGDIR="${PROJ}/build/test"; mkdir -p "${LOGDIR}"
LOG="${LOGDIR}/boot-libc.log"
WINDOW="${WINDOW:-30}"
# A mainline virt kernel (built by ../../libc/test/dynamic.sh, or boot.sh's checkout).
K="${KVIRT_ZIMAGE:-${PROJ}/build/refkernel/virt-zImage}"
[ -f "$K" ] || K="${PROJ}/build/refkernel/linux-src/arch/arm/boot/zImage"
ROOTFS_BASE="${PROJ}/build/output/initramfs-custom-custom-coreutils-static.cpio.gz"

red(){ printf '\033[31m%s\033[0m\n' "$*"; }
grn(){ printf '\033[32m%s\033[0m\n' "$*"; }

command -v "${QEMU}" >/dev/null 2>&1 || { red "error: ${QEMU} not on PATH"; exit 2; }
[ -f "$K" ] || { red "no mainline virt kernel (run libc/test/dynamic.sh once, or set KVIRT_ZIMAGE)"; exit 2; }

echo "=== building rootfs (LIBC=custom INIT=custom PACKAGES=coreutils BOARD=virt) ==="
if ! make -C "${PROJ}" rootfs LIBC=custom INIT=custom LINKAGE=static BOARD=virt \
        PACKAGES=coreutils >"${LOG}.build" 2>&1; then
	red "rootfs build failed"; tail -n 25 "${LOG}.build"; exit 2
fi
[ -f "${ROOTFS_BASE}" ] || { red "rootfs artifact missing: ${ROOTFS_BASE}"; exit 2; }

# One oneshot service that drives an orderly poweroff via our own initctl (built against the
# custom libc). Spliced into /etc/init via a second concatenated cpio (no re-pack of the base).
echo "=== splicing /etc/init/zz-poweroff.conf ==="
STG="$(mktemp -d)"; mkdir -p "${STG}/etc/init"
printf 'description poweroff probe\noneshot\nexec /sbin/initctl poweroff\n' > "${STG}/etc/init/zz-poweroff.conf"
( cd "${STG}" && find etc | cpio -o -H newc 2>/dev/null ) | gzip > "${STG}/fix.cpio.gz"
ROOTFS="${LOGDIR}/rootfs-libc+fixtures.cpio.gz"
cat "${ROOTFS_BASE}" "${STG}/fix.cpio.gz" > "${ROOTFS}"
rm -rf "${STG}"

echo "=== booting QEMU -M virt (mainline kernel + custom-libc init as PID 1), ${WINDOW}s window ==="
timeout "${WINDOW}" "${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
	-kernel "$K" -initrd "${ROOTFS}" -append "console=ttyAMA0" >"${LOG}" 2>&1
qrc=$?

pass=0; fail=0
ck(){ if grep -qF -- "$2" "${LOG}"; then grn "  ok: $1"; pass=$((pass+1)); else red "  FAIL: $1 (missing '$2')"; fail=$((fail+1)); fi; }
echo
ck "init booted as PID 1"                  "[init] booting"
ck "parsed /etc/init (scandir/fopen/fgets)" "loaded 1 service(s) from /etc/init"
ck "event loop started the oneshot (fork/execvp)" "starting zz-poweroff"
ck "initctl reached PID 1 over the control socket" "shutting down (poweroff)"
ck "orderly sync + reboot()"               "sync + poweroff"
ck "kernel powered off (PSCI)"             "reboot: Power down"
if [ "$qrc" -eq 0 ]; then grn "  ok: QEMU exited cleanly (not a timeout)"; pass=$((pass+1)); else red "  FAIL: QEMU did not exit cleanly (rc=$qrc)"; fail=$((fail+1)); fi

echo
if [ "$fail" -eq 0 ]; then grn "BOOT-LIBC: ${pass}/${pass} GREEN — custom init on the custom libc supervises on a mainline kernel"; else red "BOOT-LIBC: ${pass} ok / ${fail} FAILED"; tail -12 "${LOG}" | sed 's/^/    /'; exit 1; fi
