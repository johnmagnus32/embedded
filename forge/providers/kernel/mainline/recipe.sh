# providers/kernel/mainline/recipe.sh — mainline Linux (kconfig + pinned git). Board facts
# (defconfig, DT overlays, console) live in board/<board>/board.conf, so this stays board-agnostic.
#
#   make kernel KERNEL=mainline           # build (idempotent)
#   make kernel KERNEL=mainline CLEAN=1   # re-fetch the checkout from scratch, then rebuild
PKG_NAME=linux

PKG_FETCH=git
PKG_GIT_URL=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
PKG_GIT_URL_MIRROR=https://github.com/gregkh/linux.git
PKG_GIT_CHECKOUT=linux
PKG_VERSION=v6.12.95

PKG_HOST_DEPENDS=toolchain-glibc
PKG_ARTIFACT=out:${KERNEL_IMAGE_TARGET}

inherit kconfig
inherit devicetree

do_build() {
  # base's do_fetch already cloned/reused the pinned checkout into PKG_SRC_DIR.
  command -v git >/dev/null 2>&1 || die "git not found"
  command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
    || die "cross compiler '${CROSS_COMPILE}gcc' not on PATH — run 'make toolchain' first"
  # libelf headers are needed for objtool/CONFIG_UNWINDER.
  local t
  for t in bc bison flex perl gzip; do
    command -v "$t" >/dev/null 2>&1 || die "host build dep '$t' missing (apt: bc bison flex build-essential libssl-dev libelf-dev)"
  done
  [ -f /usr/include/libelf.h ] || log "WARN: libelf.h not found; if the build fails on elf.h, install libelf-dev"
  # sunxi_defconfig enables the ARM_SSP_PER_TASK gcc plugin, which compiles against gmp/mpfr/mpc headers.
  local h
  for h in gmp.h mpfr.h mpc.h; do
    [ -f "/usr/include/${h}" ] || log "WARN: ${h} not found; kernel gcc-plugin build needs gmp/mpfr/mpc dev headers (apt: libgmp-dev libmpfr-dev libmpc-dev)"
  done

  # The kernel Makefile requires GNU Make >= 4.0; `make toolchain` provides it on an old host.
  local MAKE_VER
  MAKE_VER="$(make --version 2>/dev/null | sed -n '1s/.*GNU Make //p')"
  case "${MAKE_VER}" in
    4.*|5.*|6.*|7.*|8.*|9.*) log "using GNU Make ${MAKE_VER}" ;;
    *) die "GNU Make on PATH is '${MAKE_VER:-none}', need >= 4.0 — run make toolchain first" ;;
  esac

  PROVIDER_RECIPE="${PROVIDER_RECIPE:-${RECIPE_DIR}/recipe.sh}"
  [ -f "${PROVIDER_RECIPE}" ] || die "provider recipe not found: ${PROVIDER_RECIPE}"
  local KERNEL_TAG; KERNEL_TAG="$(recipe_get "${PROVIDER_RECIPE}" PKG_VERSION)"
  : "${KERNEL_TAG:?recipe: PKG_VERSION missing from ${PROVIDER_RECIPE}}"
  : "${KERNEL_DEFCONFIG:?recipe: KERNEL_DEFCONFIG missing from board.conf}"

  local BOARD_DTS_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}/${UBOOT_BOARD_DT}.dts"
  local BOARD_DTS_DIR_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}"
  local BUILT_DTB_REL="arch/arm/boot/dts/${KERNEL_DTB_SUBDIR}/${KERNEL_DTB}"
  local BUILT_ZIMAGE_REL="arch/arm/boot/zImage"

  : "${PKG_SRC_DIR:?kernel do_build: PKG_SRC_DIR unset — base do_fetch should have set it}"
  cd "${PKG_SRC_DIR}"
  [ -f "${BOARD_DTS_REL}" ] || die "board DTS not found at ${BOARD_DTS_REL} (tag layout changed?)"

  # Restore the pristine board DTS so overlay appends don't stack across re-runs.
  log "restoring pristine board DTS"
  git checkout -- "${BOARD_DTS_REL}"

  # kconfig_configure: `make <defconfig>` -> fixup -> olddefconfig. verify_config_fragment then
  # asserts each forced symbol stuck — olddefconfig can silently drop one.
  kfixup() {
    local f
    for f in ${KERNEL_CONFIG_FRAGMENTS}; do
      log "applying kernel config fragment: ${f#${REPO_ROOT}/}"
      apply_config_fragment "${f}" || return 1
    done
  }
  log "make ${KERNEL_DEFCONFIG}${KERNEL_CONFIG_FRAGMENTS:+ + config fragments}"
  kconfig_configure "${KERNEL_DEFCONFIG}" kfixup
  local f
  for f in ${KERNEL_CONFIG_FRAGMENTS}; do
    verify_config_fragment "${f}" || die "kernel config fragment did not stick: ${f}"
  done

  local ov
  for ov in ${KERNEL_DTB_OVERLAYS}; do
    log "applying kernel DT overlay: ${ov}"
    apply_dtsi_overlay "${BOARD_DTS_REL}" "${ov}" "${BOARD_DIR}" "${BOARD_DTS_DIR_REL}" \
      || die "failed to apply DT overlay ${ov}"
  done

  local JOBS; JOBS="$(nproc)"
  log "building ${KERNEL_IMAGE_TARGET} + DTB (-j${JOBS}) ..."
  make -j"${JOBS}" "${KERNEL_IMAGE_TARGET}" "${KERNEL_DTB_SUBDIR}/${KERNEL_DTB}"

  [ -f "${BUILT_ZIMAGE_REL}" ] || die "build finished but ${BUILT_ZIMAGE_REL} not found"
  [ -f "${BUILT_DTB_REL}" ]    || die "build finished but ${BUILT_DTB_REL} not found"

  mkdir -p "${OUTPUT_DIR}"
  cp -f "${BUILT_ZIMAGE_REL}" "${OUTPUT_DIR}/zImage"
  cp -f "${BUILT_DTB_REL}"    "${OUTPUT_DIR}/${KERNEL_DTB}"

  local ZSIZE DSIZE
  ZSIZE="$(du -h "${OUTPUT_DIR}/zImage" | cut -f1)"
  DSIZE="$(du -h "${OUTPUT_DIR}/${KERNEL_DTB}" | cut -f1)"
  cat <<EOF

$(printf '\033[1;32m[kernel] DONE\033[0m')
  Kernel     : ${KERNEL_TAG}  (${KERNEL_DEFCONFIG})
  Console    : Linux ${KERNEL_CONSOLE}  (via ${KERNEL_DTB_OVERLAYS} overlay)
  Cores      : both A7s (U-Boot PSCI patches enable-method into this DTB at boot)
  Artifacts  : ${OUTPUT_DIR}/zImage            (${ZSIZE})
               ${OUTPUT_DIR}/${KERNEL_DTB}  (${DSIZE})

The kernel cmdline (console=${KERNEL_CONSOLE} + root=...) is set by the boot path:
the U-Boot boot script (board/*/boot.cmd) on SD, or the custom loader on NOR.
EOF
}

do_install() { :; }
