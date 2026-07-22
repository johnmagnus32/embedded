#!/usr/bin/env bash
# 02-kernel.sh — Step 2: build a mainline Linux kernel + device tree for the
# T113-S3, with the console on UART0/PE2-PE3 (same board wiring as U-Boot).
#
# What it does:
#   1. Clone mainline Linux at the pinned LTS tag (or reuse an existing checkout).
#   2. Restore the board DTS to pristine (idempotent re-runs).
#   3. make sunxi_defconfig  (32-bit ARM; enables 8250_DW UART, MMC_SUNXI, the
#      D1/T113 clock + pinctrl drivers — everything needed to reach a console).
#   4. Overlay the board DTB with scripts/uart0-console.dtsi so the kernel's
#      console is UART0/PE2-PE3 (Linux will name it ttyS0 → KERNEL_CONSOLE).
#   5. Build zImage + the board DTB; copy both to build/output/.
#
# Only CPU0 is described statically in the DT with no enable-method — that's
# expected: U-Boot's PSCI patches enable-method="psci" into this DTB at boot, so
# BOTH Cortex-A7 cores come up. Nothing to do here for SMP.
#
#   ./scripts/02-kernel.sh            # build (idempotent)
#   ./scripts/02-kernel.sh --clean    # wipe the kernel checkout first
#
# Prereq: ./scripts/00-toolchain.sh   (cross compiler)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${HERE}/env.sh"

log()  { printf '\033[1;34m[02-kernel]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[02-kernel] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

# --- preflight ---------------------------------------------------------------
command -v git >/dev/null 2>&1 || die "git not found"
command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
  || die "cross compiler '${CROSS_COMPILE}gcc' not on PATH — run ./scripts/00-toolchain.sh first"
# Kernel host build deps. libelf (elfutils) headers are needed for objtool/CONFIG_UNWINDER.
for t in bc bison flex perl gzip; do
  command -v "$t" >/dev/null 2>&1 || die "host build dep '$t' missing (apt: bc bison flex build-essential libssl-dev libelf-dev)"
done
[ -f /usr/include/libelf.h ] || log "WARN: libelf.h not found; if the build fails on elf.h, install libelf-dev"

# The kernel Makefile requires GNU Make >= 4.0. Step 0 (00-toolchain.sh) is
# responsible for providing it — either the host's own (if new enough) or a
# pinned one built into build/hostmake/, which env.sh puts on PATH. Here we just
# verify it's present so a missing Step 0 fails clearly instead of cryptically.
MAKE_VER="$(make --version 2>/dev/null | sed -n '1s/.*GNU Make //p')"
case "${MAKE_VER}" in
  4.*|5.*|6.*|7.*|8.*|9.*) log "using GNU Make ${MAKE_VER}" ;;
  *) die "GNU Make on PATH is '${MAKE_VER:-none}', need >= 4.0 — run ./scripts/00-toolchain.sh first" ;;
esac

KERNEL_SRC_DIR="${BUILD_DIR}/linux"
BOARD_DTS_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}/${UBOOT_BOARD_DT}.dts"
BOARD_DTS_DIR_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}"
CONSOLE_OVERLAY="uart0-console.dtsi"
INCLUDE_LINE="#include \"${CONSOLE_OVERLAY}\""
BUILT_DTB_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}/${KERNEL_DTB}"
BUILT_ZIMAGE_REL="arch/arm/boot/zImage"

# --- 1. clone or reuse -------------------------------------------------------
if [ "${CLEAN}" -eq 1 ]; then
  log "--clean: removing ${KERNEL_SRC_DIR}"
  rm -rf "${KERNEL_SRC_DIR}"
fi

if [ -d "${KERNEL_SRC_DIR}/.git" ]; then
  GOT_TAG="$(git -C "${KERNEL_SRC_DIR}" describe --tags 2>/dev/null || echo '?')"
  [ "${GOT_TAG}" = "${KERNEL_TAG}" ] \
    || die "existing checkout is '${GOT_TAG}', want '${KERNEL_TAG}'. Re-run with --clean."
  log "reusing kernel checkout at ${KERNEL_TAG}"
else
  log "cloning Linux ${KERNEL_TAG} (shallow) — this is the slow part"
  git_clone_pinned "${KERNEL_SRC_DIR}" "${KERNEL_TAG}" "${KERNEL_GIT_URL}" "${KERNEL_GIT_URL_MIRROR}" \
    || die "git clone failed (tried ${KERNEL_GIT_URL} and mirror)"
fi

cd "${KERNEL_SRC_DIR}"
[ -f "${BOARD_DTS_REL}" ] || die "board DTS not found at ${BOARD_DTS_REL} (tag layout changed?)"

# --- 2. restore pristine board DTS (idempotency) -----------------------------
log "restoring pristine board DTS"
git checkout -- "${BOARD_DTS_REL}"

# --- 3. defconfig + config fragment ------------------------------------------
log "make ${KERNEL_DEFCONFIG}"
make "${KERNEL_DEFCONFIG}" >/dev/null

# Disable GCC plugins: sunxi_defconfig enables CONFIG_GCC_PLUGIN_ARM_SSP_PER_TASK
# (a stack-protector hardening plugin) which requires gmp.h (libgmp-dev) on the
# host to compile the plugin .so. This host lacks it, and the plugin is not
# needed to reach a boot/console. Disable reproducibly via .config + olddefconfig.
log "disabling CONFIG_GCC_PLUGINS (host lacks gmp.h; hardening not needed for bring-up)"
./scripts/config --disable GCC_PLUGINS
make olddefconfig >/dev/null
grep -q '^# CONFIG_GCC_PLUGINS is not set' .config \
  || die "failed to disable CONFIG_GCC_PLUGINS"

# --- 4. console overlay (kernel DTB → UART0/PE2-PE3) -------------------------
log "applying UART0 console overlay to the kernel board DTB"
cp -f "${SCRIPTS_DIR}/${CONSOLE_OVERLAY}" "${BOARD_DTS_DIR_REL}/${CONSOLE_OVERLAY}"
printf '\n%s\n' "${INCLUDE_LINE}" >> "${BOARD_DTS_REL}"
grep -qF "${INCLUDE_LINE}" "${BOARD_DTS_REL}" || die "failed to append console overlay include"

# --- 5. build zImage + DTB ---------------------------------------------------
JOBS="$(nproc)"
log "building ${KERNEL_IMAGE_TARGET} + DTB (-j${JOBS}) ..."
make -j"${JOBS}" "${KERNEL_IMAGE_TARGET}" "${KERNEL_DTB_SUBDIR}/${KERNEL_DTB}"

[ -f "${BUILT_ZIMAGE_REL}" ] || die "build finished but ${BUILT_ZIMAGE_REL} not found"
[ -f "${BUILT_DTB_REL}" ]    || die "build finished but ${BUILT_DTB_REL} not found"

mkdir -p "${OUTPUT_DIR}"
cp -f "${BUILT_ZIMAGE_REL}" "${OUTPUT_DIR}/zImage"
cp -f "${BUILT_DTB_REL}"    "${OUTPUT_DIR}/${KERNEL_DTB}"

# --- report ------------------------------------------------------------------
ZSIZE="$(du -h "${OUTPUT_DIR}/zImage" | cut -f1)"
DSIZE="$(du -h "${OUTPUT_DIR}/${KERNEL_DTB}" | cut -f1)"
cat <<EOF

$(printf '\033[1;32m[02-kernel] DONE\033[0m')
  Kernel     : ${KERNEL_TAG}  (${KERNEL_DEFCONFIG})
  Console    : UART0 / PE2-PE3 → Linux ttyS0  (cmdline: console=${KERNEL_CONSOLE})
  Cores      : both A7s (U-Boot PSCI patches enable-method into this DTB at boot)
  Artifacts  : ${OUTPUT_DIR}/zImage            (${ZSIZE})
               ${OUTPUT_DIR}/${KERNEL_DTB}  (${DSIZE})

Next: Step 3 — root filesystem (BusyBox), then Step 4 — assemble + flash the SD.
The kernel cmdline (console=${KERNEL_CONSOLE} + root=...) gets set in the
U-Boot boot script / extlinux.conf during Step 4.
EOF
