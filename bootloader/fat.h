/*
 * fat.h — minimal read-only FAT16/FAT32 reader for the gameboy-v3 bootloader.
 * Finds the first FAT partition via the MBR, then loads files by name from the
 * root directory. Enough to fetch zImage / DTB / initramfs.cpio.gz.
 */
#ifndef GV3_FAT_H
#define GV3_FAT_H

#include <stdint.h>

/* Mount the first FAT partition on the SD card. Returns 0 on success. */
int fat_mount(void);

/* Load file `name` (matched case-insensitively against 8.3 short names) into
 * `buf`. Returns the byte size on success, negative on error. `max` caps the
 * copy so we never overrun the destination. */
long fat_load(const char *name, void *buf, uint32_t max);

#endif /* GV3_FAT_H */
