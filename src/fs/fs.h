/*
 * fs.h — Virtual Filesystem layer
 *
 * Like Linux's VFS or Zephyr's fs.h:
 *   - Application calls fs_open(), fs_read(), etc.
 *   - VFS dispatches to the mounted filesystem's ops
 *   - Different filesystems (tinyfs, littlefs, fat) plug in via fs_ops
 *
 * Linux:  struct file_operations + struct super_operations
 * Zephyr: struct fs_file_system_t
 * Ours:   struct fs_ops
 */

#ifndef FS_H
#define FS_H

#include "device.h"
#include <stdint.h>
#include <stddef.h>

#define FS_MAX_NAME 16

/* Forward declarations */
struct fs_mount;
struct fs_file;

/* Filesystem operations — each FS implementation provides these */
struct fs_ops {
    int  (*mount)(struct fs_mount *mp);
    int  (*format)(struct fs_mount *mp);
    int  (*open)(struct fs_file *fp, struct fs_mount *mp, const char *name);
    int  (*read)(struct fs_file *fp, void *buf, size_t len);
    int  (*write)(struct fs_file *fp, const void *data, size_t len);
    void (*close)(struct fs_file *fp);
    void (*ls)(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size));
};

/* Mount point — connects a filesystem type to a flash device */
struct fs_mount {
    const struct device *flash_dev;
    const struct fs_ops *ops;       /* which filesystem implementation */
    void *fs_data;                  /* private data for the FS impl */
    uint8_t mounted;
};

/* File handle */
struct fs_file {
    struct fs_mount *mp;
    uint8_t  index;
    uint32_t offset;
    uint8_t  open;
};

/* --- VFS API (application calls these) --- */

int  fs_mount(struct fs_mount *mp, const struct device *flash_dev,
              const struct fs_ops *ops);
int  fs_format(struct fs_mount *mp);
int  fs_open(struct fs_file *fp, struct fs_mount *mp, const char *name);
int  fs_read(struct fs_file *fp, void *buf, size_t len);
int  fs_write(struct fs_file *fp, const void *data, size_t len);
void fs_rewind(struct fs_file *fp);
void fs_close(struct fs_file *fp);
void fs_ls(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size));

/* --- Filesystem type registrations (defined in each FS impl) --- */
extern const struct fs_ops tinyfs_ops;

#endif
