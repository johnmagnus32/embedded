/*
 * vfs.c — path resolution + ELF-from-file loading (S9).
 */

#include <stdint.h>
#include "vfs.h"
#include "ramfs.h"
#include "elf.h"
#include "kmalloc.h"
#include "fs_abi.h"
#include "libk.h"

struct rf_inode *vfs_resolve(struct rf_inode *cwd, const char *path, int follow)
{
	return ramfs_lookup(cwd, path, follow);
}

int vfs_load_elf(struct rf_inode *ino, uint32_t l1_pa,
                 uint32_t *entry_out, uint32_t *brk_out)
{
	if (!ino || ino->type != RF_REG)
		return -K_EACCES;
	uint32_t sz = ino->size;
	if (sz < 52)
		return -K_ENOEXEC_S;               /* too small to be an ELF */

	/* Read the whole file into a contiguous kernel buffer. elf_load wants a
	 * flat image; ramfs stores it in pages, so we linearize once here. */
	uint8_t *buf = kmalloc(sz);
	if (!buf)
		return -K_ENOMEM;
	long n = ramfs_read(ino, 0, buf, sz);
	if (n != (long)sz) { kfree(buf); return -K_EIO_S; }

	int rc = elf_load(l1_pa, buf, sz, entry_out, brk_out);
	kfree(buf);
	return rc;
}
