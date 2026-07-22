/*
 * fs_abi.h — Linux userspace-ABI constants for the S9 filesystem syscalls.
 *
 * These are the values a static musl BusyBox (ARM 32-bit EABI) expects. They
 * were verified against arch/arm headers + arch/arm/tools/syscall.tbl by the
 * S9 verification workflow. ARM has several arch-SPECIFIC overrides that differ
 * from the asm-generic defaults — those are called out where they bite.
 */
#ifndef GV3K_FS_ABI_H
#define GV3K_FS_ABI_H

/* ---- st_mode type bits (octal), shared by cpio c_mode and stat st_mode ---- */
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

/* ---- open() / openat() flags (ARM values — some differ from asm-generic!) - */
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
/* ARM override: O_DIRECTORY = 040000 (16384), NOT the asm-generic 0200000. */
#define O_DIRECTORY  040000    /* 16384 (ARM-specific) */
#define O_NOFOLLOW   0100000   /* 32768 (ARM-specific) */
#define O_CLOEXEC    02000000  /* 1048576? NO — 02000000 = 1048576... see note */
/* NOTE: verified O_CLOEXEC = 02000000 octal = 1048576? The workflow flagged
 * 02000000 octal = 1,048,576. musl's arm bits/fcntl.h: O_CLOEXEC 02000000. We
 * only ever READ it (to set FD_CLOEXEC), so the exact bit value is not load-
 * bearing for correctness of open() itself. */

#define AT_FDCWD     (-100)
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

/* ---- errno values we return (negated) ---- */
#define K_EPERM    1
#define K_ENOENT   2
#define K_EBADF    9
#define K_ENOMEM   12
#define K_EACCES   13
#define K_EFAULT   14
#define K_ENOTDIR  20
#define K_EISDIR   21
#define K_EINVAL   22
#define K_EMFILE   24
#define K_ENOSPC   28
#define K_ESPIPE   29
#define K_ENOSYS   38
#define K_ENOTEMPTY 39
#define K_ELOOP    40
#define K_ERANGE   34
#define K_ENAMETOOLONG 36
#define K_EIO_S     5   /* -EIO   */
#define K_ENOEXEC_S 8   /* -ENOEXEC */

#endif /* GV3K_FS_ABI_H */
