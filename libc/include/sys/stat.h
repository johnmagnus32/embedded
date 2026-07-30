/*
 * sys/stat.h — stat()/fstat() over the kernel's *stat64 syscalls.
 *
 * `struct stat` is defined in the shared UAPI (gv3_abi.h) as the byte-exact ARM
 * 104-byte layout, single-sourced with the kernel. The struct tag and the stat()
 * function share the name but live in different C namespaces, so both coexist
 * (exactly as POSIX <sys/stat.h> does). The S_IF and S_IS macros come from UAPI.
 */
#ifndef _GV3_SYS_STAT_H
#define _GV3_SYS_STAT_H

#include <gv3_abi.h>      /* struct stat, S_IF*, S_IS* */
#include <sys/types.h>

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);
int lstat(const char *path, struct stat *st);

#endif /* _GV3_SYS_STAT_H */
