# boot.cmd — U-Boot boot script for gameboy-v3 (compiled to boot.scr).
#
# U-Boot's distro_bootcmd auto-scans each partition (prefixes / and /boot/) for
# "boot.scr", so dropping the compiled boot.scr at the FAT partition root makes
# U-Boot run this with no environment editing.
#
# Load addresses (${kernel_addr_r} etc.) are U-Boot's own built-in defaults for
# this SoC (32-bit sunxi, 128 MB: kernel 0x41000000, fdt 0x41800000,
# ramdisk 0x41C00000) — we reference the env vars rather than hardcoding.
#
# ${devtype}/${devnum}/${distro_bootpart} are set by distro_bootcmd to the
# device+partition this script was found on (e.g. mmc 0:1), so the same script
# works regardless of which slot the SD enumerates as.

echo "== gameboy-v3 boot.scr =="

# Console + no root= : the rootfs is the initramfs we load below (RAM-only).
setenv bootargs "console=ttyS0,115200 earlycon=on panic=10"

# Load the three artifacts from the boot partition into RAM.
load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r}  zImage
load ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r}     sun8i-t113s-mangopi-mq-r-t113.dtb
load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} initramfs.cpio.gz

# bootz needs the ramdisk SIZE for a raw (non-uImage) cpio.gz: addr:size.
# ${filesize} is set by the last 'load' (the initramfs), so capture it here.
setenv ramdisk_size ${filesize}

echo "Booting: zImage + DTB + initramfs (${ramdisk_size} bytes) ..."
bootz ${kernel_addr_r} ${ramdisk_addr_r}:${ramdisk_size} ${fdt_addr_r}
