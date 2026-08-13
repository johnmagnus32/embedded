# steps/image/recipe.sh — the image composer. MEDIA=nor|sd is an output shape within one step,
# not a selection axis. Per-MEDIA host tools are declared (PKG_HOST_DEPENDS_$(MEDIA)) and unioned
# by resolve.mk, so laziness is declared, not an inline `if MEDIA=sd`.
PKG_NAME=image

PKG_HOST_DEPENDS=
PKG_HOST_DEPENDS_sd=genimage
PKG_FETCH=none

note() { printf '\033[1;36m  note:\033[0m %s\n' "$*"; }

# Map each component to its on-disk path by asking the selected provider's recipe (via the
# runner's _artifact_path / _recipe_src_dir), rather than hardcoding. No existence check — the
# graph built these, and the cp-by-full-path in emit_*() fails loud under set -e.
resolve_artifacts() {
  : "${KERNEL_RECIPE:?resolve_artifacts: KERNEL_RECIPE unset}"
  : "${BOOTLOADER_RECIPE:?resolve_artifacts: BOOTLOADER_RECIPE unset}"

  KERNEL_ARTIFACT="$(_artifact_path "${KERNEL_RECIPE}" PKG_ARTIFACT)"
  BL_ARTIFACT="$(_artifact_path "${BOOTLOADER_RECIPE}" PKG_ARTIFACT)"      # SD path: raw @8 KiB
  BL_FEL="$(_artifact_path "${BOOTLOADER_RECIPE}" PKG_ARTIFACT_FEL)"       # NOR path: FEL-loaded image

  # Not provider-resolved like the four above: the kernel layer stages the DTB into OUTPUT_DIR and
  # the rootfs recipe always emits the canonical initramfs name, so both are fixed paths.
  DTB_ARTIFACT="${OUTPUT_DIR}/${KERNEL_DTB}"
  INITRD_ARTIFACT="${OUTPUT_DIR}/${INITRAMFS_IMAGE}"
}

# MEDIA=nor: a self-contained bundle dir (components + loader + manifest) that tools/flash.sh
# reads to do the NOR write + FEL boot on the rig.
emit_bundle() {
  log "assembling bundle -> $OUT"
  rm -rf "$OUT"; mkdir -p "$OUT"
  # Both kernels emit a zImage-shaped file (the custom kernel carries a zImage header), so one name.
  cp -f "$KERNEL_ARTIFACT" "$OUT/zImage"
  cp -f "$DTB_ARTIFACT"    "$OUT/board.dtb"
  cp -f "$INITRD_ARTIFACT" "$OUT/initramfs.cpio.gz"
  # The bootloader's FEL-loaded NOR image, staged under one name (custom: a self-contained loader;
  # uboot: U-Boot proper). flash.sh FEL-loads it and drives the rest per BOOTLOADER.
  cp -f "$BL_FEL" "$OUT/fel-loader.bin"

  local KSZ DSZ ISZ
  KSZ=$(stat -c%s "$OUT/zImage")
  DSZ=$(stat -c%s "$OUT/board.dtb")
  ISZ=$(stat -c%s "$OUT/initramfs.cpio.gz")

  cat > "$OUT/manifest.env" <<EOF
# gameboy-v3 bundle manifest — consumed by tools/flash.sh
BUNDLE_CFG="${CFG}"
BOOTLOADER="${BOOTLOADER}"
KERNEL="${KERNEL}"
ROOTFS="${ROOTFS_TAG}"
LIBC="${LIBC}"
PACKAGES="${PACKAGES}"
KERNEL_FILE="zImage"
DTB_FILE="board.dtb"
INITRD_FILE="initramfs.cpio.gz"
LOADER_FILE="fel-loader.bin"
KERNEL_SIZE=${KSZ}
DTB_SIZE=${DSZ}
INITRD_SIZE=${ISZ}
# NOR + DRAM layout — from board.conf. Must match the README's SPI-NOR layout + bootloader/nor_layout.h.
NOR_KERNEL_OFF=${NOR_KERNEL_OFF}
NOR_DTB_OFF=${NOR_DTB_OFF}
NOR_INITRD_OFF=${NOR_INITRD_OFF}
NOR_TABLE_OFF=${NOR_TABLE_OFF}
DRAM_KERNEL=${DRAM_KERNEL}
DRAM_DTB=${DRAM_DTB}
DRAM_INITRD=${DRAM_INITRD}
KERNEL_CONSOLE="${KERNEL_CONSOLE}"
EOF

  printf '\n\033[1;32m[image] DONE\033[0m  bundle: %s\n' "$OUT"
  printf '  %-18s %s\n' "kernel (zImage):" "$(du -h "$OUT/zImage" | cut -f1)"
  printf '  %-18s %s\n' "dtb:"             "$(du -h "$OUT/board.dtb" | cut -f1)"
  printf '  %-18s %s\n' "initramfs:"       "$(du -h "$OUT/initramfs.cpio.gz" | cut -f1)"
  printf '  %-18s %s\n' "loader:"          "$(du -h "$OUT/fel-loader.bin" | cut -f1)"
  printf '\nNext: flash + FEL-boot on the rig:\n'
  printf '  tools/flash.sh %s nor\n' "$OUT"
}

# MEDIA=sd: a full-disk .img (the manual card path). genimage owns all disk geometry via the
# board's genimage.cfg; we stage the inputs + run it.
emit_sd_img() {
  local GENIMAGE="${HOSTTOOLS_DIR}/bin/genimage"

  # genimage reads two staging dirs: INPUTS (raw partition images) + the FAT ROOTPATH.
  local Gdir="${BUILD_DIR}/genimage"
  local IN="${Gdir}/input" ROOT="${Gdir}/root" TMP="${Gdir}/tmp"
  rm -rf "${Gdir}"; mkdir -p "${IN}" "${ROOT}" "${TMP}" "${OUTPUT_DIR}"

  # FAT contents (names must match boot.cmd).
  cp -f "$KERNEL_ARTIFACT" "${ROOT}/zImage"
  cp -f "$DTB_ARTIFACT"    "${ROOT}/${KERNEL_DTB}"
  cp -f "$INITRD_ARTIFACT" "${ROOT}/${INITRAMFS_IMAGE}"
  if [ "$BOOTLOADER" = uboot ]; then
    # U-Boot's distro_bootcmd auto-runs /boot.scr (the custom loader ignores it, so stage only here).
    local MKIMAGE="$(_recipe_src_dir "${BOOTLOADER_RECIPE}")/tools/mkimage"
    "$MKIMAGE" -C none -A arm -T script -d "${BOARD_DIR}/boot.cmd" "${ROOT}/boot.scr" >/dev/null
  fi

  # Raw @8 KiB bootloader -> the name genimage.cfg references. BL_ARTIFACT is each provider's
  # primary artifact (custom eGON | u-boot-sunxi-with-spl), so no per-provider branch.
  cp -f "$BL_ARTIFACT" "${IN}/sdboot.bin"

  log "assembling SD image via genimage (${BOARD_DIR##*/}/genimage.cfg)"
  "${GENIMAGE}" --config "${BOARD_DIR}/genimage.cfg" \
                --inputpath "${IN}" --rootpath "${ROOT}" \
                --tmppath "${TMP}" --outputpath "${OUTPUT_DIR}" >/dev/null \
    || die "genimage failed"

  local IMG="${OUTPUT_DIR}/gameboy-v3-${CFG}-sd.img"
  mv -f "${OUTPUT_DIR}/sdcard.img" "${IMG}"
  printf '\n\033[1;32m[image] DONE\033[0m  SD image: %s  (%s)\n' "$IMG" "$(du -h "$IMG" | cut -f1)"
  note "flash: sudo dd if=$IMG of=/dev/sdX bs=4M conv=fsync status=progress  (then insert + reset)"
}

do_build() {
  # Selectors come from forge.conf (resolve.mk validated them); `:-` fallbacks are for a bare
  # standalone run outside forge.
  BOOTLOADER="${BOOTLOADER:-custom}"
  KERNEL="${KERNEL:-linux}"
  LIBC="${LIBC:-musl}"                  # manifest/log only
  PACKAGES="${PACKAGES:-busybox}"       # manifest/log only
  ROOTFS_TAG="${ROOTFS_TAG:-${LIBC}-$(echo "${PACKAGES}" | tr ' ' '+')}"
  MEDIA="${MEDIA:-nor}"

  CFG="${CFG:-${BOOTLOADER}-${KERNEL}-${ROOTFS_TAG}}"
  OUT="${OUT:-${BUNDLE:-${BUILD_DIR}/bundles/${CFG}}}"

  # Selector vocab KERNEL=linux maps to the kernel/mainline dir — the only rename.
  local _kimpl="${KERNEL}"
  [ "${_kimpl}" = linux ] && _kimpl=mainline
  KERNEL_RECIPE="${KERNEL_RECIPE:-${FORGE_ROOT}/providers/kernel/${_kimpl}/recipe.sh}"
  BOOTLOADER_RECIPE="${BOOTLOADER_RECIPE:-${FORGE_ROOT}/providers/bootloader/${BOOTLOADER}/recipe.sh}"
  export KERNEL_RECIPE BOOTLOADER_RECIPE

  log "compose: BOOTLOADER=$BOOTLOADER  KERNEL=$KERNEL  ROOTFS=$ROOTFS_TAG  MEDIA=$MEDIA  ($CFG)"
  resolve_artifacts

  case "$MEDIA" in
    nor)  emit_bundle ;;
    sd)   emit_sd_img ;;
    emmc) die "MEDIA=emmc: board has no eMMC (SPI-NOR + microSD only)" ;;
    *)    die "MEDIA must be nor|sd (got '$MEDIA')" ;;
  esac
}

do_install() { :; }
