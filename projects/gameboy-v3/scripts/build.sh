#!/usr/bin/env bash
# build.sh — build a gameboy-v3 boot configuration into a self-contained ARTIFACT
# BUNDLE that flash.sh can flash to the T113-breakout (and FEL-boot) in one go.
#
# This is the "compile + package" half of the two-script loop:
#     ./scripts/build.sh   [selectors]     ->  a bundle dir under build/bundles/
#     ./scripts/flash.sh   <bundle> <media>  ->  flash it + FEL-boot on the rig
#
# Selectors (env vars):
#   BOOTLOADER = uboot | custom      (mainline U-Boot   | our bootloader/)
#   KERNEL     = linux | custom      (mainline Linux    | our kernel/, zImage-headed)
#   ROOTFS     = busybox | scratch   (musl BusyBox      | our rootfs/ gv3libc)
#   LCD        = (unset) | ili9341   (build the kernel with the ILI9341 panel driver)
#   MEDIA      = nor | sd            (NOR: flash.sh bundle [default] ; SD: dd-able .img)
#   OUT=<dir>                        (nor bundle destination; default build/bundles/<cfg>)
#
# The two output shapes:
#   MEDIA=nor -> a self-contained BUNDLE dir (components + FEL loader + manifest.env)
#                that `flash.sh <bundle> nor` flashes + FEL-boots on the rig (the
#                remote loop). This is the default.
#   MEDIA=sd  -> a full-disk `.img` (bootloader @8KiB + a FAT boot partition) you
#                `dd` to a microSD by hand (SD is not FEL-flashable — xfel can't
#                write it). The "insert a card" path.
#
# Prereq: ./scripts/00-toolchain.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${HERE}/env.sh"

log()  { printf '\033[1;33m[build]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[build] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }
note() { printf '\033[1;36m  note:\033[0m %s\n' "$*"; }

# --- selectors ---------------------------------------------------------------
BOOTLOADER="${BOOTLOADER:-custom}"   # default to the FEL-delivered loader (the NOR loop)
KERNEL="${KERNEL:-linux}"
ROOTFS="${ROOTFS:-busybox}"
LCD="${LCD:-}"
MEDIA="${MEDIA:-nor}"                # nor -> flash.sh bundle (default) ; sd -> dd-able .img

case "$BOOTLOADER" in uboot|custom) ;; *) die "BOOTLOADER must be uboot|custom (got '$BOOTLOADER')";; esac
case "$KERNEL"     in linux|custom) ;; *) die "KERNEL must be linux|custom (got '$KERNEL')";; esac
case "$ROOTFS"     in busybox|scratch) ;; *) die "ROOTFS must be busybox|scratch (got '$ROOTFS')";; esac
case "$LCD"        in ""|ili9341) ;; *) die "LCD must be unset|ili9341 (got '$LCD')";; esac
case "$MEDIA"      in nor|sd) ;; emmc) die "MEDIA=emmc: board has no eMMC (SPI-NOR + microSD only)";; *) die "MEDIA must be nor|sd (got '$MEDIA')";; esac

CFG="${BOOTLOADER}-${KERNEL}-${ROOTFS}${LCD:+-lcd_$LCD}"
OUT="${OUT:-${BUILD_DIR}/bundles/${CFG}}"

log "config: BOOTLOADER=$BOOTLOADER  KERNEL=$KERNEL  ROOTFS=$ROOTFS  LCD=${LCD:-none}  MEDIA=$MEDIA"

if [ "$KERNEL" = linux ] && [ "$ROOTFS" = scratch ]; then
  note "KERNEL=linux + ROOTFS=scratch: scratch libc targets the custom kernel ABI; expect gaps."
fi

BOOTLOADER_DIR="${PROJECT_DIR}/bootloader"
KERNEL_DIR="${PROJECT_DIR}/kernel"
ROOTFS_DIR_SCRATCH="${PROJECT_DIR}/rootfs"

command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
  || die "cross gcc not on PATH — run ./scripts/00-toolchain.sh (or source scripts/env.sh)"

# =============================================================================
# 1. BUILD PROVIDERS  (bootloader / kernel / rootfs)
# =============================================================================
build_bootloader() {
  case "$BOOTLOADER" in
    uboot)
      log "bootloader=uboot: building via 01-uboot.sh"
      "${SCRIPTS_DIR}/01-uboot.sh"
      BL_ARTIFACT="${OUTPUT_DIR}/${UBOOT_IMAGE}"
      # flash.sh's uboot NOR path FEL-loads U-Boot proper (u-boot.bin), not the eGON
      UBOOT_PROPER="${UBOOT_SRC_DIR}/u-boot.bin"
      ;;
    custom)
      log "bootloader=custom: building bootloader/ + FEL image (0x28000)"
      make -C "${BOOTLOADER_DIR}" >/dev/null || die "custom bootloader build failed"
      make -C "${BOOTLOADER_DIR}" fel >/dev/null || die "custom bootloader (fel) build failed"
      BL_ARTIFACT="${BOOTLOADER_DIR}/build/gv3boot.egon.bin"
      BL_FEL="${BOOTLOADER_DIR}/build/gv3boot-fel-0x28000.egon.bin"
      [ -f "$BL_FEL" ] || die "custom FEL loader missing: $BL_FEL"
      ;;
  esac
  [ -f "$BL_ARTIFACT" ] || die "bootloader artifact missing: $BL_ARTIFACT"
}

build_kernel() {
  case "$KERNEL" in
    linux)
      log "kernel=linux: building via 02-kernel.sh${LCD:+ (LCD=$LCD)}"
      LCD="$LCD" "${SCRIPTS_DIR}/02-kernel.sh"
      KERNEL_ARTIFACT="${OUTPUT_DIR}/${KERNEL_IMAGE_TARGET}"
      DTB_ARTIFACT="${OUTPUT_DIR}/${KERNEL_DTB}"
      ;;
    custom)
      [ -z "$LCD" ] || note "LCD=$LCD only affects the mainline kernel; custom kernel has no DRM stack (ignored)."
      log "kernel=custom: building kernel/ (BOARD=t113 zImage-headed .bin)"
      make -C "${KERNEL_DIR}" BOARD=t113 >/dev/null || die "custom kernel build failed"
      KERNEL_ARTIFACT="${KERNEL_DIR}/build/t113/gv3kernel.bin"
      DTB_ARTIFACT="${OUTPUT_DIR}/${KERNEL_DTB}"
      ;;
  esac
  [ -f "$KERNEL_ARTIFACT" ] || die "kernel artifact missing: $KERNEL_ARTIFACT"
  [ -f "$DTB_ARTIFACT" ] || die "board DTB missing: $DTB_ARTIFACT — run ./scripts/02-kernel.sh first"
}

build_rootfs() {
  case "$ROOTFS" in
    busybox)
      log "rootfs=busybox: building via 03-rootfs.sh"
      "${SCRIPTS_DIR}/03-rootfs.sh"
      INITRD_ARTIFACT="${OUTPUT_DIR}/${INITRAMFS_IMAGE}"
      ;;
    scratch)
      log "rootfs=scratch: building rootfs/ (BOARD=t113)"
      make -C "${ROOTFS_DIR_SCRATCH}" BOARD=t113 rootfs >/dev/null || die "scratch rootfs build failed"
      INITRD_ARTIFACT="${ROOTFS_DIR_SCRATCH}/build/rootfs.cpio.gz"
      ;;
  esac
  [ -f "$INITRD_ARTIFACT" ] || die "rootfs artifact missing: $INITRD_ARTIFACT"
}

build_bootloader
build_kernel
build_rootfs

# =============================================================================
# 2. ASSEMBLE PER MEDIA
# =============================================================================

# --- MEDIA=nor: a self-contained bundle dir (components + loader + manifest) ---
# flash.sh reads this and does the NOR write + FEL boot on the rig.
emit_bundle() {
  log "assembling bundle -> $OUT"
  rm -rf "$OUT"; mkdir -p "$OUT"
  cp -f "$KERNEL_ARTIFACT" "$OUT/zImage"                 # both kernels are zImages
  cp -f "$DTB_ARTIFACT"    "$OUT/board.dtb"
  cp -f "$INITRD_ARTIFACT" "$OUT/initramfs.cpio.gz"
  if [ "$BOOTLOADER" = custom ]; then
    cp -f "$BL_FEL" "$OUT/loader-fel.bin"                # FEL-loaded @0x28000, reads NOR itself
  else
    cp -f "$UBOOT_PROPER" "$OUT/uboot-proper.bin"        # FEL-loaded @0x42e00000, sf read + bootz
  fi
  local KSZ DSZ ISZ
  KSZ=$(stat -c%s "$OUT/zImage"); DSZ=$(stat -c%s "$OUT/board.dtb"); ISZ=$(stat -c%s "$OUT/initramfs.cpio.gz")
  cat > "$OUT/manifest.env" <<EOF
# gameboy-v3 bundle manifest — consumed by flash.sh
BUNDLE_CFG="${CFG}"
BOOTLOADER="${BOOTLOADER}"
KERNEL="${KERNEL}"
ROOTFS="${ROOTFS}"
LCD="${LCD}"
KERNEL_FILE="zImage"
DTB_FILE="board.dtb"
INITRD_FILE="initramfs.cpio.gz"
LOADER_FILE="$([ "$BOOTLOADER" = custom ] && echo loader-fel.bin || echo uboot-proper.bin)"
KERNEL_SIZE=${KSZ}
DTB_SIZE=${DSZ}
INITRD_SIZE=${ISZ}
# NOR layout (must match NOR-LAYOUT.md / bootloader/nor_layout.h)
NOR_KERNEL_OFF=0x010000
NOR_DTB_OFF=0x610000
NOR_INITRD_OFF=0x620000
NOR_TABLE_OFF=0x000000
# DRAM load addresses (match the bootloader + boot.cmd)
DRAM_KERNEL=0x41000000
DRAM_DTB=0x41800000
DRAM_INITRD=0x41c00000
KERNEL_CONSOLE="${KERNEL_CONSOLE}"
EOF
  printf '\n\033[1;32m[build] DONE\033[0m  bundle: %s\n' "$OUT"
  printf '  %-18s %s\n' "kernel (zImage):" "$(du -h "$OUT/zImage" | cut -f1)"
  printf '  %-18s %s\n' "dtb:"             "$(du -h "$OUT/board.dtb" | cut -f1)"
  printf '  %-18s %s\n' "initramfs:"       "$(du -h "$OUT/initramfs.cpio.gz" | cut -f1)"
  printf '  %-18s %s\n' "loader:"          "$(basename "$(ls "$OUT"/loader-fel.bin "$OUT"/uboot-proper.bin 2>/dev/null | head -1)")"
  printf '\nNext: flash + FEL-boot on the rig:\n'
  printf '  ./scripts/flash.sh %s nor\n' "$OUT"
}

# --- MEDIA=sd: a full-disk .img you dd to a microSD (the manual card path) -----
# U-Boot (or our eGON) at the 8 KiB offset, then one FAT partition with the
# zImage + DTB + initramfs (+ a bootz boot.scr for U-Boot). Not FEL-flashable;
# this is the "insert a card" workflow. (Both kernels are zImages -> bootz.)
emit_sd_img() {
  for t in dd sfdisk mkfs.vfat mcopy truncate; do
    command -v "$t" >/dev/null 2>&1 || die "host dep '$t' missing (apt: fdisk dosfstools mtools)"
  done
  local IMG="${OUTPUT_DIR}/gameboy-v3-${CFG}-sd.img"
  local FAT="${BUILD_DIR}/boot-sd.fat"
  local PART_BYTES=$(( (SDIMAGE_SIZE_MB - SDIMAGE_PART_START_MB) * 1024 * 1024 ))
  local PART_START_SECT=$(( SDIMAGE_PART_START_MB * 1024 * 1024 / 512 ))

  log "building FAT boot partition (zImage + dtb + initramfs$([ "$BOOTLOADER" = uboot ] && echo ' + boot.scr'))"
  rm -f "$FAT"; truncate -s "$PART_BYTES" "$FAT"; mkfs.vfat -n GBV3BOOT "$FAT" >/dev/null
  MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT" "$KERNEL_ARTIFACT"  ::zImage
  MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT" "$DTB_ARTIFACT"    "::${KERNEL_DTB}"
  MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT" "$INITRD_ARTIFACT" "::${INITRAMFS_IMAGE}"
  if [ "$BOOTLOADER" = uboot ]; then
    local MKIMAGE="${UBOOT_SRC_DIR}/tools/mkimage" BOOT_SCR="${BUILD_DIR}/boot.scr"
    [ -x "$MKIMAGE" ] || die "mkimage missing — 01-uboot.sh must have run"
    "$MKIMAGE" -C none -A arm -T script -d "${SCRIPTS_DIR}/boot.cmd" "$BOOT_SCR" >/dev/null
    MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT" "$BOOT_SCR" ::boot.scr
  fi

  # For the SD path the bootloader goes at the 8 KiB offset. custom -> the eGON
  # (BL_ARTIFACT); uboot -> the combined SPL+proper (UBOOT_IMAGE).
  local BL_ON_DISK="$BL_ARTIFACT"
  [ "$BOOTLOADER" = custom ] || BL_ON_DISK="${OUTPUT_DIR}/${UBOOT_IMAGE}"
  [ -f "$BL_ON_DISK" ] || die "SD bootloader image missing: $BL_ON_DISK"

  log "assembling ${IMG##*/} (${SDIMAGE_SIZE_MB} MiB)"
  rm -f "$IMG"; truncate -s "${SDIMAGE_SIZE_MB}M" "$IMG"
  printf 'label: dos\nstart=%d, type=0c\n' "$PART_START_SECT" | sfdisk --quiet "$IMG"
  dd if="$BL_ON_DISK" of="$IMG" bs=1024 seek="${SDIMAGE_UBOOT_SEEK_KB}" conv=notrunc status=none
  dd if="$FAT"        of="$IMG" bs=512  seek="${PART_START_SECT}"       conv=notrunc status=none
  printf '\n\033[1;32m[build] DONE\033[0m  SD image: %s  (%s)\n' "$IMG" "$(du -h "$IMG" | cut -f1)"
  note "flash: sudo dd if=$IMG of=/dev/sdX bs=4M conv=fsync status=progress  (then insert + reset)"
}

case "$MEDIA" in
  nor) emit_bundle ;;
  sd)  emit_sd_img ;;
esac
