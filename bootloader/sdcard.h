/*
 * sdcard.h — minimal SD-card driver for the gameboy-v3 bootloader (T113-S3).
 * Brings up the SMHC0 controller + the SD card, then reads 512-byte blocks by
 * LBA via PIO (CPU drains the FIFO — same approach as U-Boot's SPL, no DMA).
 */
#ifndef GV3_SDCARD_H
#define GV3_SDCARD_H

#include <stdint.h>

/* Init the SMHC0 controller and the inserted SD card.  Returns 0 on success. */
int sd_init(void);

/* Read `count` 512-byte blocks starting at LBA `lba` into `buf`.
 * Returns 0 on success, negative on error. */
int sd_read_blocks(uint32_t lba, uint32_t count, void *buf);

#define SD_BLOCK_SIZE  512u

#endif /* GV3_SDCARD_H */
