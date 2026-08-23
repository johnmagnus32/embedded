#!/usr/bin/env bash
# init/test/boot.sh — boot the CUSTOM init (INIT=custom, the C supervisor) as REAL PID 1 on a
# MAINLINE kernel under QEMU, and assert it supervises correctly.
#
# This tests our init against the OSS baseline (mainline kernel + musl), NOT against our custom
# kernel — the swappability contract: a custom component is proven by running it against the real
# thing. It is the init's analog of kernel/test/golden.sh (which tests the custom KERNEL with a
# minimal shell init). It runs the C init's event loop for real: real PID 1, real mounts, real
# signalfd/timerfd/epoll on an arm kernel — things the host harness can't exercise.
#
# Needs a mainline kernel that boots QEMU `-M virt` (multi_v7_defconfig; the product's sunxi build
# lacks the virt hardware). Point KVIRT_ZIMAGE at it, or drop it at the default path below.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INITDIR="$(cd "${HERE}/.." && pwd)"                  # <repo>/init
REPO="$(cd "${INITDIR}/.." && pwd)"
PROJ="${GV3_PRODUCT:-${REPO}/projects/gameboy-v3}"
# A mainline kernel that boots QEMU -M virt (PL011 + virtio). The refkernel checkout carries one
# (v6.12.95, multi_v7-ish); override KVIRT_ZIMAGE to point elsewhere.
KVIRT_ZIMAGE="${KVIRT_ZIMAGE:-${PROJ}/build/refkernel/linux-src/arch/arm/boot/zImage}"
ROOTFS_BASE="${PROJ}/build/output/initramfs-musl-custom-busybox-static.cpio.gz"
QEMU="${QEMU:-qemu-system-arm}"
LOGDIR="${INITDIR}/build/test"; mkdir -p "${LOGDIR}"
LOG="${LOGDIR}/boot.log"
WINDOW="${WINDOW:-14}"                               # seconds to wait for the poweroff self-exit (probe fires ~7s)

red(){ printf '\033[31m%s\033[0m\n' "$*"; }
grn(){ printf '\033[32m%s\033[0m\n' "$*"; }

command -v "${QEMU}" >/dev/null 2>&1 || { red "error: ${QEMU} not on PATH"; exit 2; }
[ -f "${KVIRT_ZIMAGE}" ] || {
  red "error: no multi_v7 virt kernel at ${KVIRT_ZIMAGE}"
  red "       build one (mainline, multi_v7_defconfig) and set KVIRT_ZIMAGE, then re-run."
  exit 2; }

echo "=== building base rootfs (INIT=custom PACKAGES=busybox BOARD=virt) ==="
if ! make -C "${PROJ}" rootfs KERNEL=mainline LIBC=musl INIT=custom \
        PACKAGES=busybox BOARD=virt >"${LOG}.build" 2>&1; then
  red "rootfs build failed"; tail -n 25 "${LOG}.build"; exit 2
fi
[ -f "${ROOTFS_BASE}" ] || { red "rootfs artifact missing: ${ROOTFS_BASE}"; exit 2; }

# The test-service .conf live WITH this test (init/test/conf/), not in the product package catalog.
# Splice them into /etc/init via a SECOND cpio concatenated onto the rootfs — the kernel unpacks
# multiple initramfs archives in sequence, so no re-pack of the base image is needed.
echo "=== splicing test fixtures (init/test/conf/*.conf -> /etc/init) ==="
STG="$(mktemp -d)"; mkdir -p "${STG}/etc/init"
cp "${HERE}/conf/"*.conf "${STG}/etc/init/"
( cd "${STG}" && find etc | cpio -o -H newc 2>/dev/null ) | gzip > "${STG}/fixtures.cpio.gz"
ROOTFS="${LOGDIR}/rootfs+fixtures.cpio.gz"
cat "${ROOTFS_BASE}" "${STG}/fixtures.cpio.gz" > "${ROOTFS}"
rm -rf "${STG}"

echo "=== booting QEMU -M virt (mainline multi_v7 kernel + C init as PID 1), ${WINDOW}s window ==="
# -net none: skip the default virtio-net device (needs efi-virtio.rom, absent in some QEMU builds).
"${QEMU}" -M virt -cpu cortex-a7 -m 128M -nographic -no-reboot -net none \
  -kernel "${KVIRT_ZIMAGE}" -initrd "${ROOTFS}" -append "console=ttyAMA0" >"${LOG}" 2>&1 &
qpid=$!
# The test fixtures fire `initctl poweroff` mid-run; QEMU exits (PSCI SYSTEM_OFF) when init
# powers off. Poll for that self-exit; if it doesn't happen within the window, kill it (a hang).
self_exited=0; waited=0
while [ "${waited}" -lt "${WINDOW}" ]; do
  kill -0 "${qpid}" 2>/dev/null || { self_exited=1; break; }
  sleep 1; waited=$((waited + 1))
done
[ "${self_exited}" = 0 ] && { kill "${qpid}" 2>/dev/null; wait "${qpid}" 2>/dev/null; }

echo "----- console -----"; cat "${LOG}"; echo "-------------------"

# ---- assertions ----
fail=0
ok(){ grn "  ok: $*"; }
no(){ red "  FAIL: $*"; fail=1; }
has(){ grep -qF -- "$1" "${LOG}"; }
cnt(){ grep -cF -- "$1" "${LOG}"; }
line(){ grep -n -- "$1" "${LOG}" | head -1 | cut -d: -f1; }

# Real PID-1 on a real kernel — the things the host harness can't prove. The negatives are gated on
# the banner so an empty/failed boot can't vacuously "pass" them.
booted=0; has 'booting' && booted=1
[ "${booted}" = 1 ]                                        && ok "C init booted as PID 1"                          || no "init banner missing (did not boot)"
[ "${booted}" = 1 ] && ! has 'not PID 1'                   && ok "runs as real PID 1"                              || no "not real PID 1 (or did not boot)"
[ "${booted}" = 1 ] && ! has 'FATAL'                       && ok "event loop up (signalfd/timerfd/epoll ok on arm — no fatal)" || no "hit a FATAL (missing event primitives?) or did not boot"
[ "${booted}" = 1 ] && ! has 'Operation not permitted'     && ok "early mounts succeeded (real root)"              || no "early mounts hit EPERM (or did not boot)"
# Supervision behaviour:
[ "$(cnt 'INITTEST-ONCE')" = 1 ]        && ok "oneshot ran exactly once"        || no "oneshot (count=$(cnt 'INITTEST-ONCE'))"
[ "$(cnt 'INITTEST-RESPAWN')" -ge 2 ]   && ok "respawn works ($(cnt 'INITTEST-RESPAWN')x)" || no "respawn (<2)"
has 'INITTEST-DEP' && [ -n "$(line 'starting respawn')" ] && [ -n "$(line 'starting dep')" ] \
  && [ "$(line 'starting respawn')" -lt "$(line 'starting dep')" ] \
  && ok "after ordering (respawn before dep)"                                  || no "after ordering"
has 'gated: ready' && has 'INITTEST-AFTERREADY' \
  && [ -n "$(line 'gated: ready')" ] && [ -n "$(line 'starting afterready')" ] \
  && [ "$(line 'gated: ready')" -lt "$(line 'starting afterready')" ] \
  && ok "ready-gating (afterready held until gated published its socket)"      || no "ready gating"
# initctl + shutdown (the zzctl.conf fixture runs `initctl status` then `initctl poweroff`):
[ "${booted}" = 1 ] && has 'pid='                          && ok "initctl status replied over /run/initctl.sock"  || no "initctl status (socket)"
[ "${booted}" = 1 ] && has 'shutting down (poweroff)'      && ok "initctl poweroff -> orderly shutdown"           || no "shutdown not triggered"
[ "${self_exited}" = 1 ]                                   && ok "clean poweroff (QEMU exited via PSCI SYSTEM_OFF)" || no "no poweroff (killed at window — hang?)"

echo
if [ "${fail}" = 0 ]; then
  grn "INIT BOOT TEST: ALL GREEN — the C init supervises as real PID 1 on a mainline kernel"; exit 0
else
  red "INIT BOOT TEST: FAILURES (full console: ${LOG})"; exit 1
fi
