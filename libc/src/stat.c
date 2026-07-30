/*
 * stat.c — stat/fstat/lstat over the kernel's *stat64 syscalls. The kernel
 * fills a `struct stat` (the shared UAPI 104-byte ARM layout) directly.
 */
#include <sys/stat.h>
#include <fcntl.h>
#include "syscall_internal.h"

int fstat(int fd, struct stat *st)
{
	return (int)__ret(__sys2(SYS_fstat64, fd, st));
}

int stat(const char *path, struct stat *st)
{
	/* our kernel exposes stat64(path, statbuf) directly */
	return (int)__ret(__sys2(SYS_stat64, path, st));
}

int lstat(const char *path, struct stat *st)
{
	return (int)__ret(__sys2(SYS_lstat64, path, st));
}
