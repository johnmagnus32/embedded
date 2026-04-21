/*
 * fs.c — VFS dispatch layer
 *
 * All fs_*() calls go through here and dispatch to the mounted
 * filesystem's ops. The application never calls tinyfs directly.
 *
 * Like Linux's fs/read_write.c → calls f_op->read()
 * Like Zephyr's subsys/fs/fs.c → calls fs->open()
 */

#include "fs.h"

int fs_mount(struct fs_mount *mp, const struct device *flash_dev,
             const struct fs_ops *ops)
{
    mp->flash_dev = flash_dev;
    mp->ops = ops;
    mp->fs_data = (void *)0;
    mp->mounted = 0;

    int rc = ops->mount(mp);
    if (rc == 0) mp->mounted = 1;
    return rc;
}

int fs_format(struct fs_mount *mp)
{
    return mp->ops->format(mp);
}

int fs_open(struct fs_file *fp, struct fs_mount *mp, const char *name)
{
    fp->mp = mp;
    fp->offset = 0;
    fp->open = 0;
    return mp->ops->open(fp, mp, name);
}

int fs_read(struct fs_file *fp, void *buf, size_t len)
{
    return fp->mp->ops->read(fp, buf, len);
}

int fs_write(struct fs_file *fp, const void *data, size_t len)
{
    return fp->mp->ops->write(fp, data, len);
}

void fs_rewind(struct fs_file *fp)
{
    fp->offset = 0;
}

void fs_close(struct fs_file *fp)
{
    fp->mp->ops->close(fp);
}

void fs_ls(struct fs_mount *mp, void (*cb)(const char *name, uint32_t size))
{
    mp->ops->ls(mp, cb);
}
