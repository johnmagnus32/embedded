/*
 * sys/stat.h — stat()/fstat() over the kernel's *stat64 syscalls.
 *
 * `struct stat` is defined in the shared UAPI (abi.h) as the byte-exact ARM
 * 104-byte layout, single-sourced with the kernel. The struct tag and the stat()
 * function share the name but live in different C namespaces, so both coexist
 * (exactly as POSIX <sys/stat.h> does). The S_IF and S_IS macros come from UAPI.
 */
#ifndef _LIBC_SYS_STAT_H
#define _LIBC_SYS_STAT_H

#include <abi.h>      /* struct stat, S_IF*, S_IS* */
#include <sys/types.h>

/* POSIX permission bits (octal). The S_IF* type bits live in the shared UAPI (abi.h); these
 * mode bits are a libc concern and belong here — a complete libc must expose them (e.g. libgcc's
 * gcov support opens .gcda files with S_IRUSR|S_IWUSR). */
#ifndef S_IRWXU
#define S_IRWXU 0000700
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRWXG 0000070
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IRWXO 0000007
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001
#endif

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);
int lstat(const char *path, struct stat *st);
int mkdir(const char *path, mode_t mode);

#endif /* _LIBC_SYS_STAT_H */
