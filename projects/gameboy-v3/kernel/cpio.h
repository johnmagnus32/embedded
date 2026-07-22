/*
 * cpio.h — parse a "newc" (magic 070701) cpio archive into the ramfs (S9).
 *
 * The initramfs is a newc cpio (the format gen_init_cpio / `cpio -H newc`
 * produce and the Linux init/initramfs.c parser consumes). We walk it and
 * recreate every entry — directories, regular files (with data), symlinks,
 * device nodes — as ramfs inodes.
 *
 * "newc" layout, per entry:
 *   [110-byte ASCII header][name (c_namesize bytes, incl NUL)][pad to 4]
 *   [file data (c_filesize bytes)][pad to 4]
 * All header fields are 8-char ASCII hex except the 6-char magic. Archive ends
 * with an entry named "TRAILER!!!".
 */
#ifndef GV3K_CPIO_H
#define GV3K_CPIO_H

#include <stdint.h>

/* Unpack the newc cpio at [base, base+size) into the (already-initialised)
 * ramfs root. Returns the number of entries created, or negative on a format
 * error. Paths in the archive are relative (e.g. "bin/sh", "dev/console"); we
 * create them under the ramfs root, making intermediate dirs as needed. */
int cpio_load(const void *base, uint32_t size);

#endif /* GV3K_CPIO_H */
