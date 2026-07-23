/*
 * dirent.h — opendir/readdir/closedir over getdents64(2).
 *
 * The on-the-wire record `struct gv3_dirent64` is the shared UAPI layout. We
 * expose the classic `DIR*` + `struct dirent` API on top: opendir() opens the
 * directory fd and buffers a chunk of getdents64 output; readdir() hands back
 * one entry at a time, refilling as needed.
 */
#ifndef _GV3_DIRENT_H
#define _GV3_DIRENT_H

#include <gv3_abi.h>     /* struct gv3_dirent64, DT_* */
#include <sys/types.h>

/* The POSIX-facing entry (a stable subset of the kernel record). */
struct dirent {
	unsigned long long d_ino;
	unsigned char      d_type;
	char               d_name[256];
};

typedef struct __dirstream DIR;

DIR           *opendir(const char *path);
struct dirent *readdir(DIR *d);
int            closedir(DIR *d);

#endif /* _GV3_DIRENT_H */
