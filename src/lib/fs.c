/*
 * fs.c — TinyFS: a simple flat filesystem on NOR flash
 *
 * Flash layout:
 *   Sector 0:  Directory table (array of dir_entry structs)
 *   Sector 1:  File 0 data (up to 4KB)
 *   Sector 2:  File 1 data
 *   ...
 *   Sector 15: File 14 data
 *
 * Each dir_entry stores: name, size, used flag.
 * The directory is read into RAM on mount and flushed on write/close.
 *
 * This is similar to how early DOS FAT worked — a fixed directory
 * at a known location, files in contiguous sectors.
 *
 * Maps to Zephyr's subsys/fs/ (LittleFS or FAT backend).
 */

#include "fs.h"
#include "flash.h"
#include <stdint.h>

/* On-flash directory entry */
struct dir_entry {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  used;
    uint8_t  _pad[3];
};

/* In-RAM copy of the directory */
static struct dir_entry dir_table[FS_MAX_FILES];

#define DIR_SECTOR      0
#define DATA_SECTOR(i)  ((i) + 1)
#define DIR_SIZE        (sizeof(dir_table))

/* --- Helpers --- */

static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

static void strcopy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int dir_flush(struct fs_mount *mp)
{
    /* Erase directory sector and rewrite */
    flash_erase(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, FS_SECTOR_SIZE);
    flash_write(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE,
                dir_table, DIR_SIZE);
    return 0;
}

static int dir_load(struct fs_mount *mp)
{
    return flash_read(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE,
                      dir_table, DIR_SIZE);
}

/* --- Public API --- */

int fs_format(struct fs_mount *mp)
{
    /* Erase directory sector — all entries become 0xFF (unused) */
    flash_erase(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, FS_SECTOR_SIZE);

    /* Write empty directory */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        dir_table[i].used = 0;
        dir_table[i].size = 0;
        dir_table[i].name[0] = '\0';
    }
    return dir_flush(mp);
}

int fs_mount(struct fs_mount *mp, const struct device *flash_dev)
{
    mp->flash_dev = flash_dev;

    /* Load directory from flash */
    dir_load(mp);

    /* Check if formatted: first entry's used field should be 0 or 1.
     * If flash is erased (0xFF), we need to format first. */
    if (dir_table[0].used == 0xFF) {
        fs_format(mp);
    }

    mp->mounted = 1;
    return 0;
}

int fs_open(struct fs_file *fp, struct fs_mount *mp, const char *name)
{
    fp->mp = mp;
    fp->offset = 0;
    fp->open = 0;

    /* Search for existing file */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (dir_table[i].used && streq(dir_table[i].name, name)) {
            fp->file_index = i;
            fp->open = 1;
            return 0;
        }
    }

    /* Create new file in first free slot */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!dir_table[i].used) {
            strcopy(dir_table[i].name, name, FS_MAX_NAME);
            dir_table[i].size = 0;
            dir_table[i].used = 1;
            dir_flush(mp);

            /* Erase the data sector for this file */
            flash_erase(mp->flash_dev,
                        DATA_SECTOR(i) * FS_SECTOR_SIZE,
                        FS_SECTOR_SIZE);

            fp->file_index = i;
            fp->open = 1;
            return 0;
        }
    }

    return -1;  /* no free slots */
}

int fs_write(struct fs_file *fp, const void *data, size_t len)
{
    if (!fp->open) return -1;

    uint32_t sector_off = DATA_SECTOR(fp->file_index) * FS_SECTOR_SIZE;
    uint32_t max_write = FS_SECTOR_SIZE - fp->offset;
    if (len > max_write) len = max_write;

    flash_write(fp->mp->flash_dev, sector_off + fp->offset, data, len);
    fp->offset += len;

    /* Update size in directory */
    if (fp->offset > dir_table[fp->file_index].size) {
        dir_table[fp->file_index].size = fp->offset;
        dir_flush(fp->mp);
    }

    return (int)len;
}

int fs_read(struct fs_file *fp, void *buf, size_t len)
{
    if (!fp->open) return -1;

    uint32_t file_size = dir_table[fp->file_index].size;
    uint32_t remaining = file_size - fp->offset;
    if (len > remaining) len = remaining;
    if (len == 0) return 0;

    uint32_t sector_off = DATA_SECTOR(fp->file_index) * FS_SECTOR_SIZE;
    flash_read(fp->mp->flash_dev, sector_off + fp->offset, buf, len);
    fp->offset += len;

    return (int)len;
}

void fs_rewind(struct fs_file *fp)
{
    fp->offset = 0;
}

void fs_close(struct fs_file *fp)
{
    fp->open = 0;
}

void fs_ls(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size))
{
    (void)mp;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (dir_table[i].used) {
            cb(dir_table[i].name, dir_table[i].size);
        }
    }
}
