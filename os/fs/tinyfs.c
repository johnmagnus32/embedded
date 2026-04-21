/*
 * tinyfs.c — TinyFS: a simple flat filesystem
 *
 * Plugs into the VFS via tinyfs_ops.
 * Like how LittleFS plugs into Zephyr's fs layer via littlefs_fs.c,
 * or how ext4 plugs into Linux's VFS via ext4/super.c.
 *
 * Flash layout:
 *   Sector 0:  Directory table (15 entries)
 *   Sector 1+: One sector per file (4KB max each)
 */

#include "fs.h"
#include "flash.h"
#include <stdint.h>

#define FS_MAX_FILES    15
#define FS_SECTOR_SIZE  4096
#define DIR_SECTOR      0
#define DATA_SECTOR(i)  ((i) + 1)

struct dir_entry {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  used;
    uint8_t  _pad[3];
};

static struct dir_entry dir_table[FS_MAX_FILES];
#define DIR_SIZE (sizeof(dir_table))

static int streq(const char *a, const char *b)
{
    while (*a && *b) { if (*a++ != *b++) return 0; }
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
    flash_erase(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, FS_SECTOR_SIZE);
    flash_write(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, dir_table, DIR_SIZE);
    return 0;
}

/* --- fs_ops implementation --- */

static int tinyfs_mount(struct fs_mount *mp)
{
    flash_read(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, dir_table, DIR_SIZE);

    if (dir_table[0].used == 0xFF) {
        /* Unformatted — format now */
        for (int i = 0; i < FS_MAX_FILES; i++) {
            dir_table[i].used = 0;
            dir_table[i].size = 0;
            dir_table[i].name[0] = '\0';
        }
        dir_flush(mp);
    }
    return 0;
}

static int tinyfs_format(struct fs_mount *mp)
{
    flash_erase(mp->flash_dev, DIR_SECTOR * FS_SECTOR_SIZE, FS_SECTOR_SIZE);
    for (int i = 0; i < FS_MAX_FILES; i++) {
        dir_table[i].used = 0;
        dir_table[i].size = 0;
        dir_table[i].name[0] = '\0';
    }
    return dir_flush(mp);
}

static int tinyfs_open(struct fs_file *fp, struct fs_mount *mp, const char *name)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (dir_table[i].used && streq(dir_table[i].name, name)) {
            fp->index = i;
            fp->open = 1;
            return 0;
        }
    }

    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!dir_table[i].used) {
            strcopy(dir_table[i].name, name, FS_MAX_NAME);
            dir_table[i].size = 0;
            dir_table[i].used = 1;
            dir_flush(mp);
            flash_erase(mp->flash_dev, DATA_SECTOR(i) * FS_SECTOR_SIZE, FS_SECTOR_SIZE);
            fp->index = i;
            fp->open = 1;
            return 0;
        }
    }
    return -1;
}

static int tinyfs_write(struct fs_file *fp, const void *data, size_t len)
{
    if (!fp->open) return -1;
    uint32_t off = DATA_SECTOR(fp->index) * FS_SECTOR_SIZE + fp->offset;
    uint32_t max = FS_SECTOR_SIZE - fp->offset;
    if (len > max) len = max;

    flash_write(fp->mp->flash_dev, off, data, len);
    fp->offset += len;

    if (fp->offset > dir_table[fp->index].size) {
        dir_table[fp->index].size = fp->offset;
        dir_flush(fp->mp);
    }
    return (int)len;
}

static int tinyfs_read(struct fs_file *fp, void *buf, size_t len)
{
    if (!fp->open) return -1;
    uint32_t remaining = dir_table[fp->index].size - fp->offset;
    if (len > remaining) len = remaining;
    if (len == 0) return 0;

    uint32_t off = DATA_SECTOR(fp->index) * FS_SECTOR_SIZE + fp->offset;
    flash_read(fp->mp->flash_dev, off, buf, len);
    fp->offset += len;
    return (int)len;
}

static void tinyfs_close(struct fs_file *fp)
{
    fp->open = 0;
}

static void tinyfs_ls(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size))
{
    (void)mp;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (dir_table[i].used)
            cb(dir_table[i].name, dir_table[i].size);
    }
}

/* Register this filesystem type */
const struct fs_ops tinyfs_ops = {
    .mount  = tinyfs_mount,
    .format = tinyfs_format,
    .open   = tinyfs_open,
    .read   = tinyfs_read,
    .write  = tinyfs_write,
    .close  = tinyfs_close,
    .ls     = tinyfs_ls,
};
