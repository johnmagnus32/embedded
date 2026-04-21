/*
 * fs.h — Simple filesystem API
 *
 * Like Zephyr's fs.h / Linux's VFS:
 *   fs_mount()  → mount a filesystem on a flash device
 *   fs_open()   → open a file by name
 *   fs_write()  → write data
 *   fs_read()   → read data
 *   fs_close()  → close file
 *
 * Our "tinyfs" is a flat filesystem:
 *   - Fixed number of files (directory is a table at sector 0)
 *   - Each file gets one sector (4KB max)
 *   - No subdirectories, no fragmentation, no wear leveling
 *
 * Real LittleFS adds: wear leveling, power-loss resilience,
 * dynamic allocation, directories, CRC checksums.
 */

#ifndef LIB_FS_H
#define LIB_FS_H

#include "device.h"
#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILES     15    /* files per filesystem */
#define FS_MAX_NAME      16    /* max filename length */
#define FS_SECTOR_SIZE   4096  /* one sector per file */

/* Mount point — connects a filesystem to a flash device */
struct fs_mount {
    const struct device *flash_dev;
    uint8_t mounted;
};

/* File handle */
struct fs_file {
    struct fs_mount *mp;
    uint8_t  file_index;    /* index in directory table */
    uint32_t offset;        /* current read/write position */
    uint8_t  open;
};

/* Initialize and mount filesystem on a flash device */
int fs_mount(struct fs_mount *mp, const struct device *flash_dev);

/* Format the filesystem (erases everything) */
int fs_format(struct fs_mount *mp);

/* Open a file. Creates it if it doesn't exist. */
int fs_open(struct fs_file *fp, struct fs_mount *mp, const char *name);

/* Write data at current position */
int fs_write(struct fs_file *fp, const void *data, size_t len);

/* Read data at current position */
int fs_read(struct fs_file *fp, void *buf, size_t len);

/* Seek to beginning */
void fs_rewind(struct fs_file *fp);

/* Close file */
void fs_close(struct fs_file *fp);

/* List all files (calls callback for each) */
void fs_ls(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size));

#endif
