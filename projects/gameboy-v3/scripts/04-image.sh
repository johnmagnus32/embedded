#!/usr/bin/env bash
# 04-image.sh — Step 4: assemble a flashable microSD image from the Step 1-3
# artifacts. Produces build/output/gameboy-v3-sd.img — dd it to a card and boot.
#
# Image layout (all-on-SD, Phase 1):
#   offset 0        : partition table (MBR)
#   offset 8 KiB    : U-Boot SPL + U-Boot proper (u-boot-sunxi-with-spl.bin)
#   offset 1 MiB    : partition 1, FAT — zImage, DTB, initramfs.cpio.gz, boot.scr
#
# The BROM reads the SPL at 8 KiB; U-Boot's distro_bootcmd then auto-scans the
# FAT partition, finds /boot.scr, and runs it (load 3 files → bootz). No root
# needed to build the image: sfdisk partitions a plain file, mkfs.vfat + mcopy
# populate a FAT filesystem image, and dd splices the pieces together.
#
#   ./scripts/04-image.sh            # build the image
#
# Prereqs: steps 0-3 (the four artifacts must be in build/output/).
# This step builds an IMAGE only; it does NOT touch any real device. Flashing to
# a card is a separate, manual, root-required step — see the printed instructions
# and FLASH.md.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${HERE}/env.sh"

log()  { printf '\033[1;34m[04-image]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[04-image] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# --- preflight ---------------------------------------------------------------
for t in dd sfdisk mkfs.vfat mcopy truncate; do
  command -v "$t" >/dev/null 2>&1 || die "host dep '$t' missing (apt: fdisk dosfstools mtools)"
done
MKIMAGE="${UBOOT_SRC_DIR}/tools/mkimage"
[ -x "${MKIMAGE}" ] || die "mkimage not found at ${MKIMAGE} — run ./scripts/01-uboot.sh first"

UBOOT_BIN="${OUTPUT_DIR}/${UBOOT_IMAGE}"
ZIMAGE="${OUTPUT_DIR}/zImage"
DTB="${OUTPUT_DIR}/${KERNEL_DTB}"
INITRD="${OUTPUT_DIR}/${INITRAMFS_IMAGE}"
for f in "${UBOOT_BIN}" "${ZIMAGE}" "${DTB}" "${INITRD}"; do
  [ -f "$f" ] || die "missing artifact: $f — run the earlier steps first"
done

# --- 1. compile boot.cmd -> boot.scr -----------------------------------------
log "compiling boot.cmd -> boot.scr"
BOOT_SCR="${BUILD_DIR}/boot.scr"
"${MKIMAGE}" -C none -A arm -T script -d "${SCRIPTS_DIR}/boot.cmd" "${BOOT_SCR}" >/dev/null \
  || die "mkimage boot.scr failed"

# --- 2. build the FAT boot-partition filesystem image ------------------------
# Size the FAT fs to hold the payload + slack, rounded up; then mkfs + mcopy.
PART_BYTES=$(( (SDIMAGE_SIZE_MB - SDIMAGE_PART_START_MB) * 1024 * 1024 ))
FATIMG="${BUILD_DIR}/boot.fat"
log "creating FAT boot filesystem ($(( PART_BYTES / 1024 / 1024 )) MiB)"
rm -f "${FATIMG}"
truncate -s "${PART_BYTES}" "${FATIMG}"
mkfs.vfat -n GBV3BOOT "${FATIMG}" >/dev/null || die "mkfs.vfat failed"

log "copying artifacts into the boot filesystem"
# mcopy writes into the FAT image without mounting (no root needed).
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${ZIMAGE}"   ::zImage
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${DTB}"      "::${KERNEL_DTB}"
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${INITRD}"   "::${INITRAMFS_IMAGE}"
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${BOOT_SCR}" ::boot.scr

# --- 3. assemble the full-disk image -----------------------------------------
IMG="${OUTPUT_DIR}/${SDIMAGE}"
log "assembling ${SDIMAGE} (${SDIMAGE_SIZE_MB} MiB)"
rm -f "${IMG}"
truncate -s "${SDIMAGE_SIZE_MB}M" "${IMG}"

# Partition table: one primary FAT (type 0x0c = FAT32 LBA) starting at 1 MiB.
# The pre-1MiB gap holds the MBR (sector 0) and U-Boot (from 8 KiB).
PART_START_SECT=$(( SDIMAGE_PART_START_MB * 1024 * 1024 / 512 ))
log "writing partition table (p1 FAT @ ${SDIMAGE_PART_START_MB} MiB)"
printf 'label: dos\nstart=%d, type=0c\n' "${PART_START_SECT}" \
  | sfdisk --quiet "${IMG}" || die "sfdisk failed"

# Splice U-Boot in at the 8 KiB offset. conv=notrunc so we overwrite in place
# without truncating the image; U-Boot (~500 KB) fits well below the 1 MiB mark.
log "writing U-Boot at ${SDIMAGE_UBOOT_SEEK_KB} KiB offset"
dd if="${UBOOT_BIN}" of="${IMG}" bs=1024 seek="${SDIMAGE_UBOOT_SEEK_KB}" conv=notrunc status=none

# Splice the populated FAT filesystem into partition 1.
log "writing boot filesystem into partition 1"
dd if="${FATIMG}" of="${IMG}" bs=512 seek="${PART_START_SECT}" conv=notrunc status=none

# --- report ------------------------------------------------------------------
IMG_SIZE="$(du -h "${IMG}" | cut -f1)"
cat <<EOF

$(printf '\033[1;32m[04-image] DONE\033[0m')
  Image      : ${IMG}  (${IMG_SIZE})
  Layout     : MBR | U-Boot @ ${SDIMAGE_UBOOT_SEEK_KB}KiB | FAT p1 @ ${SDIMAGE_PART_START_MB}MiB (zImage, DTB, initramfs, boot.scr)
  Console    : UART0 / PE2(TX) PE3(RX) @ 115200 8N1

  FLASH IT (root required, and DOUBLE-CHECK the device node!):
    lsblk                                  # find your card, e.g. /dev/sdX or /dev/mmcblk0
    sudo dd if=${IMG} of=/dev/sdX bs=4M conv=fsync status=progress
    sync

  Then wire a 3.3V USB-serial adapter to the t113-breakout:
    adapter RX -> PE2 (H11-18, board TX)
    adapter TX -> PE3 (H10-18, board RX)
    adapter GND -> any header GND
    picocom -b 115200 /dev/ttyUSB0     (or: screen /dev/ttyUSB0 115200)

  Power the board via USB-C. You should see: SPL -> U-Boot -> boot.scr ->
  kernel log -> the gameboy-v3 initramfs banner -> a shell prompt.

  See FLASH.md for the full procedure, expected output, and troubleshooting.
EOF
