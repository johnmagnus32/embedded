/*
 * dirent.h — struct linux_dirent64 for getdents64 (nr 217).
 *
 * VERIFIED (S9 workflow) against include/uapi/linux/dirent.h / fs/readdir.c:
 *   d_ino(u64)@0, d_off(s64)@8, d_reclen(u16)@16, d_type(u8)@18, d_name[]@19.
 * offsetof(d_name) = 19. Each record's d_reclen is rounded UP to a multiple of
 * 8 so the next record's u64 d_ino stays aligned:
 *   reclen = round_up(19 + namelen + 1, 8)   (+1 for the NUL, then 8-align).
 * The kernel writes d_off of record N as the byte offset where record N+1
 * begins (a cookie for lseek-on-dir); we use a simple running offset.
 */
#ifndef GV3K_DIRENT_H
#define GV3K_DIRENT_H

#include <stdint.h>

struct linux_dirent64 {
	uint64_t d_ino;
	int64_t  d_off;
	uint16_t d_reclen;
	uint8_t  d_type;
	char     d_name[];       /* NUL-terminated, then pad to d_reclen */
};

#define DIRENT64_NAMEOFF  19u
#define DIRENT64_RECLEN(namelen)  (((DIRENT64_NAMEOFF + (namelen) + 1) + 7u) & ~7u)

_Static_assert(__builtin_offsetof(struct linux_dirent64, d_name) == 19,
               "linux_dirent64 d_name must be at offset 19");

#endif /* GV3K_DIRENT_H */
