/*
 * kstat.h — the ARM 32-bit 'struct stat64' the kernel fills for
 * fstat64/stat64/lstat64/fstatat64 (nr 197/195/196/327).
 *
 * VERIFIED byte-for-byte (S9 workflow) against arch/arm/include/uapi/asm/stat.h.
 * This is the ARM-SPECIFIC packed layout — NOT the asm-generic one, NOT x86-32.
 * Total size 104 bytes. Two fields matter to get right:
 *   - There are TWO inode fields: __st_ino (32-bit, offset 12, legacy/broken)
 *     AND the real st_ino (64-bit, offset 96, at the very END). Fill both.
 *   - ARM EABI 8-aligns the u64 st_size/st_blocks, inserting anonymous 4-byte
 *     holes at offsets 44 and 60. We make them explicit (__hole1/__hole2) and
 *     _Static_assert the offsets so a layout mistake fails the build.
 */
#ifndef GV3K_KSTAT_H
#define GV3K_KSTAT_H

#include <stdint.h>

struct kstat64 {
	uint64_t st_dev;         /* 0  */
	uint32_t __pad0;         /* 8  */
	uint32_t __st_ino;       /* 12 legacy 32-bit inode */
	uint32_t st_mode;        /* 16 (4 bytes on ARM stat64, not short) */
	uint32_t st_nlink;       /* 20 */
	uint32_t st_uid;         /* 24 */
	uint32_t st_gid;         /* 28 */
	uint64_t st_rdev;        /* 32 */
	uint32_t __pad3;         /* 40 */
	uint32_t __hole1;        /* 44 (EABI pad before 8-aligned st_size) */
	int64_t  st_size;        /* 48 */
	uint32_t st_blksize;     /* 56 */
	uint32_t __hole2;        /* 60 (EABI pad before 8-aligned st_blocks) */
	uint64_t st_blocks;      /* 64 (512-byte units) */
	uint32_t st_atime;       /* 72 */
	uint32_t st_atime_nsec;  /* 76 */
	uint32_t st_mtime;       /* 80 */
	uint32_t st_mtime_nsec;  /* 84 */
	uint32_t st_ctime;       /* 88 */
	uint32_t st_ctime_nsec;  /* 92 */
	uint64_t st_ino;         /* 96 real 64-bit inode */
};

/* Fail the build if the layout ever drifts from the verified ARM ABI. */
_Static_assert(sizeof(struct kstat64) == 104, "kstat64 must be 104 bytes (ARM)");
_Static_assert(__builtin_offsetof(struct kstat64, st_mode) == 16, "st_mode@16");
_Static_assert(__builtin_offsetof(struct kstat64, st_size) == 48, "st_size@48");
_Static_assert(__builtin_offsetof(struct kstat64, st_blocks) == 64, "st_blocks@64");
_Static_assert(__builtin_offsetof(struct kstat64, st_ino) == 96, "st_ino@96");

#endif /* GV3K_KSTAT_H */
