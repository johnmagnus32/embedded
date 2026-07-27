#!/usr/bin/env bash
# 04-image-custom.sh — assemble a flashable microSD image that boots via our
# OWN custom bootloader (bootloader/) instead of mainline U-Boot, while still
# running the open-source Linux kernel + BusyBox initramfs from Steps 2/3.
#
# This is "test config 2": custom bootloader + open-source kernel + initramfs.
# It is IDENTICAL to 04-image.sh's output except:
#   * offset 8 KiB holds  bootloader/build/gv3boot.egon.bin  (not U-Boot)
#   * the FAT partition has NO boot.scr (that's a U-Boot artifact; our bootloader
#     reads zImage/DTB/initramfs by name and sets bootargs itself)
#
# Image layout:
#   offset 0        : partition table (MBR)
#   offset 8 KiB    : gv3boot.egon.bin   (our eGON SPL — BROM loads + runs it)
#   offset 1 MiB    : partition 1, FAT — zImage, DTB, initramfs.cpio.gz
#
# Our bootloader (Stage 3/4): DRAM init → SD/FAT read of the 3 files into DRAM
# (kernel 0x41000000, dtb 0x41800000, initramfs 0x41c00000) → fdt_set_bootargs →
# jump to Linux. It needs the DTB's /memory node (baked in via uart0-console.dtsi)
# and inserts /chosen/bootargs at runtime (no placeholder required anymore).
#
#   ./scripts/04-image-custom.sh          # build the image
#
# Prereqs:
#   ./scripts/02-kernel.sh   (zImage + DTB)
#   ./scripts/03-rootfs.sh   (initramfs.cpio.gz)
#   the bootloader (built here automatically if the eGON is missing)
# Depends on NEITHER U-Boot nor mkimage.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${HERE}/env.sh"

log()  { printf '\033[1;35m[04-custom]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[04-custom] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# --- names/paths specific to the custom-bootloader image ---------------------
BOOTLOADER_DIR="${PROJECT_DIR}/bootloader"
EGON="${BOOTLOADER_DIR}/build/gv3boot.egon.bin"
CUSTOM_SDIMAGE="gameboy-v3-custom-sd.img"

# --- preflight ---------------------------------------------------------------
for t in dd sfdisk mkfs.vfat mcopy truncate make; do
  command -v "$t" >/dev/null 2>&1 || die "host dep '$t' missing (apt: fdisk dosfstools mtools)"
done

# The kernel + rootfs artifacts (open source) must already be built.
ZIMAGE="${OUTPUT_DIR}/zImage"
DTB="${OUTPUT_DIR}/${KERNEL_DTB}"
INITRD="${OUTPUT_DIR}/${INITRAMFS_IMAGE}"
for f in "${ZIMAGE}" "${DTB}" "${INITRD}"; do
  [ -f "$f" ] || die "missing artifact: $f — run ./scripts/02-kernel.sh and 03-rootfs.sh first"
done

# --- 1. build the custom bootloader eGON (if needed) -------------------------
# env.sh has already put the cross toolchain on PATH. The bootloader Makefile
# uses ${CROSS_COMPILE} (same pinned glibc toolchain as U-Boot/kernel).
if [ ! -f "${EGON}" ]; then
  log "building custom bootloader (eGON not found)"
  command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
    || die "cross compiler '${CROSS_COMPILE}gcc' not on PATH — run ./scripts/00-toolchain.sh"
  make -C "${BOOTLOADER_DIR}" >/dev/null || die "bootloader build failed"
fi
[ -f "${EGON}" ] || die "eGON still missing at ${EGON}"

# sanity: the BROM expects a valid eGON.BT0 header at the start of the image.
if ! head -c 16 "${EGON}" | grep -q 'eGON'; then
  die "${EGON} lacks an eGON.BT0 header — is it the right file?"
fi
EGON_SZ="$(stat -c%s "${EGON}")"
log "custom bootloader: ${EGON}  (${EGON_SZ} bytes)"

# --- 2. build the FAT boot-partition filesystem (no boot.scr) ----------------
PART_BYTES=$(( (SDIMAGE_SIZE_MB - SDIMAGE_PART_START_MB) * 1024 * 1024 ))
FATIMG="${BUILD_DIR}/boot-custom.fat"
log "creating FAT boot filesystem ($(( PART_BYTES / 1024 / 1024 )) MiB)"
rm -f "${FATIMG}"
truncate -s "${PART_BYTES}" "${FATIMG}"
mkfs.vfat -n GBV3BOOT "${FATIMG}" >/dev/null || die "mkfs.vfat failed"

log "copying kernel + DTB + initramfs into the boot filesystem"
# Filenames MUST match what bootloader/main.c loads by name (KERNEL_NAME etc.).
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${ZIMAGE}" ::zImage
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${DTB}"    "::${KERNEL_DTB}"
MTOOLS_SKIP_CHECK=1 mcopy -i "${FATIMG}" "${INITRD}" "::${INITRAMFS_IMAGE}"

# --- 3. assemble the full-disk image -----------------------------------------
IMG="${OUTPUT_DIR}/${CUSTOM_SDIMAGE}"
log "assembling ${CUSTOM_SDIMAGE} (${SDIMAGE_SIZE_MB} MiB)"
rm -f "${IMG}"
truncate -s "${SDIMAGE_SIZE_MB}M" "${IMG}"

PART_START_SECT=$(( SDIMAGE_PART_START_MB * 1024 * 1024 / 512 ))
log "writing partition table (p1 FAT @ ${SDIMAGE_PART_START_MB} MiB)"
printf 'label: dos\nstart=%d, type=0c\n' "${PART_START_SECT}" \
  | sfdisk --quiet "${IMG}" || die "sfdisk failed"

# eGON at the 8 KiB offset (sector 16) — exactly where U-Boot would go; the BROM
# reads the eGON header there. conv=notrunc: overwrite in place, keep image size.
# The eGON (~14 KB) ends far below the 1 MiB partition start, so it can't clobber
# the FAT.
log "writing custom bootloader eGON at ${SDIMAGE_UBOOT_SEEK_KB} KiB offset"
dd if="${EGON}" of="${IMG}" bs=1024 seek="${SDIMAGE_UBOOT_SEEK_KB}" conv=notrunc status=none

log "writing boot filesystem into partition 1"
dd if="${FATIMG}" of="${IMG}" bs=512 seek="${PART_START_SECT}" conv=notrunc status=none

# --- report ------------------------------------------------------------------
IMG_SIZE="$(du -h "${IMG}" | cut -f1)"
cat <<EOF

$(printf '\033[1;32m[04-custom] DONE\033[0m')
  Image      : ${IMG}  (${IMG_SIZE})
  Bootloader : CUSTOM  (bootloader/build/gv3boot.egon.bin, ${EGON_SZ} bytes)
  Payload    : open-source zImage + DTB + initramfs (Steps 2/3), FAT p1 @ ${SDIMAGE_PART_START_MB}MiB
  Layout     : MBR | eGON @ ${SDIMAGE_UBOOT_SEEK_KB}KiB | FAT p1 (zImage, DTB, initramfs — NO boot.scr)
  Console    : UART0 / PE2(TX) PE3(RX) @ 115200 8N1

  Boot chain : BROM -> our eGON SPL -> DRAM init -> SD/FAT load -> DTB patch -> Linux
  This tests "config 2": custom bootloader + open-source kernel + BusyBox initramfs.

  FLASH IT (root required, DOUBLE-CHECK the device node!):
    lsblk
    sudo dd if=${IMG} of=/dev/sdX bs=4M conv=fsync status=progress
    sync

  Expect on serial: the "gameboy-v3 custom bootloader" banner (Stage 1-4), then
  the kernel log, then the initramfs banner + a shell — see bootloader/README.md.
EOF
