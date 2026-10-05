/*
 * common/arread.h — a validated view of an ar archive, shared by the tools that read archives (nm, objcopy/strip).
 * The GNU/SysV format: "!<arch>\n", then members of a 60-byte header + bytes (padded to even length); member "/" is
 * the symbol index, "//" the long-name table ("/N" names point into it, each ending at "/\n"). A THIN archive
 * ("!<thin>\n", GNU `ar T`) stores only those two: each member is a file of its own, its name a path relative to
 * the archive's directory. ar_open checks every header once and dies (the tool's die()) on the first inconsistency.
 */
#ifndef OS_ARREAD_H
#define OS_ARREAD_H
#include "elf.h"

typedef struct {
	const char *path; u8 *data; long size; int thin;
	const char *longtab; long longsz;                /* the "//" table (NULL if none)  */
	const u8 *index; long index_size;               /* the "/" symbol index (NULL if none) */
} Archive;

typedef struct {
	long off;                                        /* its header's offset in the archive */
	char name[1024];                                 /* as stored (a thin member: its path, relative to the archive) */
	long size;                                       /* its bytes */
	const u8 *data;                                  /* in the archive image; NULL for a thin member (a file: ar_member_path) */
} ArMember;

int  ar_is_archive(const u8 *d, long size);          /* "!<arch>\n" or "!<thin>\n" */
void ar_open(Archive *ar, const char *path, u8 *data, long size);
int  ar_next(const Archive *ar, long *pos, ArMember *m);   /* the next real member from *pos (start: 8); 0 = no more */
char *ar_member_path(const Archive *ar, const ArMember *m); /* a thin member's file (malloc'd): the archive's dir + name */

void die(const char *fmt, ...);                      /* provided by each tool (its own prefix) */
#endif
