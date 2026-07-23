/*
 * gv3_abi.h — the userspace<->kernel ABI TYPES and CONSTANTS (ARM 32-bit EABI).
 *
 * Companion to gv3_syscalls.h (which holds the numbers). This is the rest of the
 * UAPI: the flag constants (S_IF, O_, AT_, SEEK_, F_, DT_, PROT_, MAP_) and the
 * byte-exact struct layouts (stat64, linux_dirent64, termios, winsize) that both
 * sides must agree on. Single source of truth: the kernel's syscall handlers and
 * the rootfs libc both consume THIS file, so a layout/flag change propagates to
 * both and they cannot drift.
 *
 * VERIFIED byte-for-byte against arch/arm/include/uapi/asm/{stat,ioctls}.h,
 * include/uapi/linux/dirent.h, and asm-generic/termios.h (S9/S10 workflows).
 * ARM has several arch-SPECIFIC overrides vs asm-generic (O_DIRECTORY=040000,
 * the packed 104-byte stat64 with two 64-bit inode fields); those are the ones
 * that bite, and they're captured here.
 *
 * Pure types + constants, no code — safe to include from kernel or userspace.
 */
#ifndef GV3_UAPI_ABI_H
#define GV3_UAPI_ABI_H

#include <stdint.h>

/* ---- st_mode type bits (octal); shared by cpio c_mode and stat st_mode ---- */
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)

/* ---- open()/openat() flags (ARM values; some differ from asm-generic!) ---- */
#define O_RDONLY     0
#define O_WRONLY     1
#define O_RDWR       2
#define O_ACCMODE    3
#define O_CREAT      0100      /* 64   */
#define O_EXCL       0200      /* 128  */
#define O_NOCTTY     0400      /* 256  */
#define O_TRUNC      01000     /* 512  */
#define O_APPEND     02000     /* 1024 */
#define O_NONBLOCK   04000     /* 2048 */
#define O_DIRECTORY  040000    /* 16384 — ARM-specific, NOT asm-generic 0200000 */
#define O_NOFOLLOW   0100000   /* 32768 — ARM-specific */
#define O_CLOEXEC    02000000

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_EMPTY_PATH       0x1000

/* ---- lseek / _llseek whence ---- */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ---- fcntl / fcntl64 commands ---- */
#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_DUPFD_CLOEXEC 1030
#define FD_CLOEXEC 1

/* ---- getdents64 d_type ---- */
#define DT_UNKNOWN 0
#define DT_CHR     2
#define DT_DIR     4
#define DT_REG     8
#define DT_LNK     10

/* ---- mmap prot/flags (asm-generic on ARM) ---- */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_TYPE       0x0f
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20

/* ---- struct stat64 (ARM-specific packed 104-byte layout) -----------------
 * TWO inode fields: __st_ino (32-bit @12, legacy) AND st_ino (64-bit @96, the
 * real one at the very end). EABI 8-aligns the u64 st_size/st_blocks, so there
 * are anonymous 4-byte holes at 44 and 60 — made explicit as __hole1/__hole2. */
/* Bare standard tag `struct stat`: a struct tag and the stat() FUNCTION live in
 * different C namespaces, so they coexist (this is exactly what POSIX
 * <sys/stat.h> does) — unlike a macro alias, which would rewrite both. */
struct stat {
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
_Static_assert(sizeof(struct stat) == 104, "stat64 must be 104 bytes (ARM)");
_Static_assert(__builtin_offsetof(struct stat, st_mode) == 16, "st_mode@16");
_Static_assert(__builtin_offsetof(struct stat, st_size) == 48, "st_size@48");
_Static_assert(__builtin_offsetof(struct stat, st_blocks) == 64, "st_blocks@64");
_Static_assert(__builtin_offsetof(struct stat, st_ino) == 96, "st_ino@96");

/* ---- struct linux_dirent64 (getdents64) ---------------------------------- */
struct gv3_dirent64 {
	uint64_t d_ino;
	int64_t  d_off;
	uint16_t d_reclen;
	uint8_t  d_type;
	char     d_name[];       /* NUL-terminated, then pad to d_reclen */
};
#define GV3_DIRENT64_NAMEOFF  19u
#define GV3_DIRENT64_RECLEN(namelen)  (((GV3_DIRENT64_NAMEOFF + (namelen) + 1) + 7u) & ~7u)
_Static_assert(__builtin_offsetof(struct gv3_dirent64, d_name) == 19,
               "linux_dirent64 d_name must be at offset 19");

/* ---- struct termios (ARM == asm-generic; NCCS=19, 36 bytes) --------------- */
#define GV3_NCCS 19
struct gv3_termios {
	uint32_t c_iflag, c_oflag, c_cflag, c_lflag;
	uint8_t  c_line;
	uint8_t  c_cc[GV3_NCCS];
};

/* ---- struct winsize (TIOCGWINSZ) ----------------------------------------- */
struct gv3_winsize {
	uint16_t ws_row;
	uint16_t ws_col;
	uint16_t ws_xpixel;
	uint16_t ws_ypixel;
};

/* ---- terminal ioctl request codes ---- */
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TIOCGPGRP   0x540F
#define TIOCSPGRP   0x5410
#define TIOCSCTTY   0x540E
#define TIOCNOTTY   0x5422
#define FIONREAD    0x541B

#endif /* GV3_UAPI_ABI_H */
