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
# Bare "earlycon" (no =value) is the correct/safe form: it makes the kernel scan
# the DTB /chosen/stdout-path and register the snps,dw-apb-uart earlycon, reading
# reg-shift/io-width from the node and INHERITING U-Boot's already-correct 115200
# (uartclk stays 0 so the divisor isn't reprogrammed). Note: "earlycon=on" is a
# silent no-op (matches no driver), and "earlycon=uart8250,...,<baud>" reprograms
# the divisor off the wrong 1.8432MHz base (real sunxi UART clk is 24MHz) → garbage.
setenv bootargs "console=ttyS0,115200 earlycon panic=10"

# Load the three artifacts from the boot partition into RAM.
load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r}  zImage
load ${devtype} ${devnum}:${distro_bootpart} ${fdt_addr_r}     sun8i-t113s-mangopi-mq-r-t113.dtb
load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} initramfs.cpio.gz

# bootz needs the ramdisk SIZE for a raw (non-uImage) cpio.gz: addr:size.
# ${filesize} is set by the last 'load' (the initramfs), so capture it here.
setenv ramdisk_size ${filesize}

# CRITICAL: the board DTB ships a long PLACEHOLDER /chosen/bootargs (reserved for
# the custom bootloader to overwrite in place). U-Boot's bootz does NOT reliably
# replace an existing bootargs property, so without this the kernel would read the
# PLACEHOLDER as its cmdline — no console=, no earlycon -> SILENT hang. We force
# our bootargs into the FDT /chosen node explicitly before booting. (Verified on
# silicon: kernel loaded + "Starting kernel" but zero output until this was set.)
fdt addr ${fdt_addr_r}
fdt set /chosen bootargs "${bootargs}"

echo "Booting: zImage + DTB + initramfs (${ramdisk_size} bytes) ..."
bootz ${kernel_addr_r} ${ramdisk_addr_r}:${ramdisk_size} ${fdt_addr_r}
