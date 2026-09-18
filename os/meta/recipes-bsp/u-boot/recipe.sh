# providers/bootloader/uboot/recipe.sh — mainline U-Boot (kconfig + pinned git). Board facts
# (defconfig, CONS_INDEX/SPI-NOR fragments, DT overlays) live in machine.conf.
#
#   make bootloader BOOTLOADER=uboot            # build (idempotent)
#   make bootloader BOOTLOADER=uboot CLEAN=1    # re-fetch the checkout from scratch, then rebuild
PKG_NAME=uboot
PKG_CLASS=target
PKG_PROVIDES=bootloader
PKG_FILEDEPS="${DEVICETREE_DIR} ${LAYER_FILES} ${MACHINE_CONF}"   # DT overlays + u-boot config fragments + machine.conf — build inputs the recipehash must catch
PKG_DEPLOY="u-boot-sunxi-with-spl.bin:bootloader.bin u-boot.bin:fel-loader.bin tools/mkimage:mkimage"

PKG_FETCH=git
PKG_GIT_URL=https://source.denx.de/u-boot/u-boot.git
PKG_GIT_URL_MIRROR=https://github.com/u-boot/u-boot.git
PKG_GIT_CHECKOUT=u-boot
PKG_VERSION=v2026.04

# Declared only here → the python venv is provisioned iff U-Boot is in the build.
PKG_HOST_DEPENDS="cross-cc binman-venv"

inherit kconfig
inherit devicetree

do_build() {
  # base's do_fetch already cloned/reused the pinned checkout into PKG_SRC_DIR.
  command -v git >/dev/null 2>&1 || die "git not found"
  command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
    || die "cross compiler '${CROSS_COMPILE}gcc' not on PATH (it's provisioned as a build dependency via cross-cc)"
  # dtc is intentionally absent — U-Boot builds its own.
  local t
  for t in bison flex bc swig; do
    command -v "$t" >/dev/null 2>&1 || die "host build dep '$t' missing (apt: bison flex bc swig libssl-dev)"
  done

  # U-Boot's sunxi+SPL build runs binman/pylibfdt, which the toolchain's own python lacks.
  # binman-venv provisioned it into PYENV_DIR — front-load it on PATH + point $PYTHON at it.
  if [ -x "${PYENV_DIR}/bin/python3" ]; then
    case ":${PATH}:" in *":${PYENV_DIR}/bin:"*) : ;; *) PATH="${PYENV_DIR}/bin:${PATH}"; export PATH ;; esac
    PYTHON="${PYENV_DIR}/bin/python3"; export PYTHON
  fi
  python3 -c 'import setuptools, elftools, yaml' 2>/dev/null \
    || die "build venv missing binman deps (setuptools/pyelftools/pyyaml) — run via 'make bootloader BOOTLOADER=uboot'"

  # Our own facts (PKG_VERSION) are already shell vars — set at the top of this recipe, which the
  # runner sourced before calling do_build — so use them directly, don't re-parse the file.
  # Only UBOOT_DEFCONFIG needs a guard: it's a machine.conf fact, not declared here.
  : "${UBOOT_DEFCONFIG:?recipe: UBOOT_DEFCONFIG missing from machine.conf}"
  local UBOOT_TAG="${PKG_VERSION}" UBOOT_IMAGE=u-boot-sunxi-with-spl.bin

  local BOARD_DTS_REL="dts/upstream/src/arm/allwinner/${UBOOT_BOARD_DT}.dts"
  local BOARD_DTS_DIR_REL="dts/upstream/src/arm/allwinner"

  : "${PKG_SRC_DIR:?u-boot do_build: PKG_SRC_DIR unset — base do_fetch should have set it}"
  cd "${PKG_SRC_DIR}"
  [ -f "${BOARD_DTS_REL}" ] || die "board DTS not found at ${BOARD_DTS_REL} (tag layout changed?)"

  # Restore the pristine board DTS so overlay #includes don't stack across re-runs.
  log "restoring pristine board DTS"
  git checkout -- "${BOARD_DTS_REL}"

  # Fragment order is load-bearing: provider no-tools FIRST (lives beside this recipe), then the
  # board's CONS_INDEX + SPI-NOR fragment(s) from machine.conf — bare filenames resolved against
  # LAYER_FILES/ (recipes-bsp/u-boot/files/). Within uboot.config MTD must precede SPI_FLASH — its
  # menu is `if MTD`.
  local _UBOOT_FRAGMENTS="${RECIPE_DIR}/uboot-notools.config" f
  for f in ${UBOOT_CONFIG_FRAGMENTS:-}; do _UBOOT_FRAGMENTS="${_UBOOT_FRAGMENTS} ${LAYER_FILES}/${f}"; done
  ufixup() {
    local f
    for f in ${_UBOOT_FRAGMENTS}; do
      log "applying u-boot config fragment: ${f#${REPO_ROOT}/}"
      apply_config_fragment "${f}" || return 1
    done
  }
  log "make ${UBOOT_DEFCONFIG} + config fragments"
  kconfig_configure "${UBOOT_DEFCONFIG}" ufixup
  local f
  for f in ${_UBOOT_FRAGMENTS}; do
    verify_config_fragment "${f}" || die "u-boot config fragment did not stick: ${f}"
  done

  # uart0-console.dtsi (DM-serial reads stdout-path from the DTB) + nor-spi.dtsi (declares flash@0
  # so `sf probe` finds the W25Q128 — CMD_SF needs the chip in the control DTB).
  local ov
  for ov in ${UBOOT_DTB_OVERLAYS}; do
    log "applying u-boot DT overlay: ${ov}"
    apply_dtsi_overlay "${BOARD_DTS_REL}" "${ov}" "${DEVICETREE_DIR}" "${BOARD_DTS_DIR_REL}" \
      || die "failed to apply DT overlay ${ov}"
  done

  local JOBS; JOBS="$(nproc)"
  log "building U-Boot (-j${JOBS}) ..."
  make -j"${JOBS}"

  [ -f "${UBOOT_IMAGE}" ] || die "build finished but ${UBOOT_IMAGE} not found"
  printf '\n\033[1;32m[uboot] built\033[0m  %s (%s)\n' "${UBOOT_TAG}" "${UBOOT_DEFCONFIG}"
}
