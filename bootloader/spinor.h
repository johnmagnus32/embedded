/*
 * spinor.h — minimal SPI-NOR (W25Q128) reader for the gameboy-v3 bootloader.
 * Brings up SPI0 (PC2-PC5) and reads raw byte ranges by NOR offset via PIO
 * (standard 0x03 READ command). Read-only: the bootloader only loads the boot
 * components (kernel/DTB/initramfs) that were pre-flashed with `xfel spinor
 * write` per ../projects/gameboy-v3/README.md (SPI-NOR layout).
 */
#ifndef GV3_SPINOR_H
#define GV3_SPINOR_H

#include <stdint.h>

/* Init SPI0 + probe the NOR (reads JEDEC ID). Returns 0 on success. */
int spinor_init(void);

/* Read `len` bytes from NOR byte-offset `addr` into `buf`.
 * Returns 0 on success, negative on error. */
int spinor_read(uint32_t addr, void *buf, uint32_t len);

#endif /* GV3_SPINOR_H */
