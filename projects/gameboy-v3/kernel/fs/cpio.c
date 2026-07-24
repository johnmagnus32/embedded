/*
 * cpio.c — newc (magic "070701") cpio parser -> ramfs (S9).
 *
 * Verified format (S9 workflow, all claims confirmed):
 *   - 110-byte header: c_magic(6)="070701" then 13 fields of 8 ASCII-hex chars:
 *     c_ino c_mode c_uid c_gid c_nlink c_mtime c_filesize c_devmajor c_devminor
 *     c_rdevmajor c_rdevminor c_namesize c_check.
 *   - name follows the header (c_namesize bytes, INCLUDING the trailing NUL).
 *   - name is padded with NUL to a 4-byte boundary (measured from archive start);
 *     since the header is 110 ≡ 2 (mod 4), the pad accounts for that offset.
 *   - file data follows, c_filesize bytes, also padded to 4.
 *   - symlink target is the file DATA (length c_filesize, may or may not carry a
 *     trailing NUL depending on producer — we NUL-terminate ourselves).
 *   - archive ends with an entry named "TRAILER!!!" (c_namesize=11).
 */

#include <stdint.h>
#include "cpio.h"
#include "ramfs.h"
#include "fs_abi.h"
#include "libk.h"

/* parse `w` ASCII-hex chars (upper or lower case) into a u32. */
static uint32_t hex(const char *p, int w)
{
	uint32_t v = 0;
	for (int i = 0; i < w; i++) {
		char c = p[i];
		uint32_t d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else d = 0;
		v = (v << 4) | d;
	}
	return v;
}

#define ALIGN4(x)  (((x) + 3u) & ~3u)

/* Create every intermediate directory in `path` (relative, no leading '/'),
 * returning the parent dir of the final component and writing that component
 * name into `leaf`. E.g. "bin/busybox" -> mkdir "bin", leaf="busybox". */
static struct rf_inode *make_parents(const char *path, char *leaf, uint32_t leafcap)
{
	struct rf_inode *dir = ramfs_root();
	const char *p = path;
	while (*p == '/') p++;

	for (;;) {
		char comp[64];
		uint32_t i = 0;
		while (p[i] && p[i] != '/' && i < sizeof(comp) - 1) { comp[i] = p[i]; i++; }
		comp[i] = '\0';
		/* component too long for our name buffer -> abort rather than silently
		 * split its tail into a bogus nested directory. */
		if (p[i] && p[i] != '/')
			return 0;
		const char *next = p + i;
		while (*next == '/') next++;

		if (!*next) {                        /* comp is the final component */
			strlcpy_(leaf, comp, leafcap);
			return dir;
		}
		/* intermediate: descend, creating the dir if absent */
		struct rf_inode *child = ramfs_child(dir, comp);
		if (!child)
			child = ramfs_mkdir(dir, comp, 0755);
		if (!child || child->type != RF_DIR)
			return 0;
		dir = child;
		p = next;
	}
}

int cpio_load(const void *base, uint32_t size)
{
	const uint8_t *b = base;
	uint32_t off = 0;
	int count = 0;

	for (;;) {
		if (off + 110 > size)
			return -1;                       /* truncated */
		const char *h = (const char *)(b + off);
		if (memcmp(h, "070701", 6) != 0) {
			/* tolerate NUL padding between/after entries (do_reset) */
			if (h[0] == '\0') { off += 4; if (off >= size) break; continue; }
			return -2;                       /* bad magic */
		}

		uint32_t mode     = hex(h + 14, 8);
		uint32_t filesize = hex(h + 54, 8);
		uint32_t rmaj     = hex(h + 78, 8);
		uint32_t rmin     = hex(h + 86, 8);
		uint32_t namesize = hex(h + 94, 8);

		const char *name = h + 110;
		if (off + 110 + namesize > size)
			return -3;

		/* end-of-archive marker */
		if (namesize == 11 && memcmp(name, "TRAILER!!!", 10) == 0)
			break;

		/* advance offset past header+name (4-aligned from archive start) */
		uint32_t data_off = ALIGN4(off + 110 + namesize);
		const uint8_t *data = b + data_off;
		if (data_off + filesize > size)
			return -4;

		/* Skip "." and absolute-ish oddities; create the entry. Names are
		 * relative ("bin/sh", "dev/console"); "." is the root dir itself. */
		if (!(name[0] == '.' && name[1] == '\0')) {
			char leaf[64];
			struct rf_inode *dir = make_parents(name, leaf, sizeof(leaf));
			if (dir && leaf[0]) {
				if (S_ISDIR(mode)) {
					if (!ramfs_child(dir, leaf))
						ramfs_mkdir(dir, leaf, mode & 07777);
				} else if (S_ISLNK(mode)) {
					char tgt[128];
					uint32_t n = filesize < sizeof(tgt) - 1 ? filesize : sizeof(tgt) - 1;
					memcpy(tgt, data, n);
					tgt[n] = '\0';           /* producer may omit NUL; we add it */
					ramfs_mklnk(dir, leaf, tgt);
				} else if (S_ISCHR(mode)) {
					ramfs_mknod(dir, leaf, mode, rmaj, rmin);
				} else if (S_ISREG(mode)) {
					struct rf_inode *f = ramfs_child(dir, leaf);
					if (!f)
						f = ramfs_mkreg(dir, leaf, mode & 07777);
					/* hardlink earlier copies carry filesize=0; only write data
					 * when present (last copy / sole copy). */
					if (f && filesize)
						ramfs_write(f, 0, data, filesize);
				}
				/* (block devs / fifos / sockets: ignored for S9) */
			}
			count++;
		}

		off = ALIGN4(data_off + filesize);
		if (off >= size)
			break;
	}
	return count;
}
