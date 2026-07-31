/*
 * unistd.c — POSIX file + process syscall wrappers over the raw svc layer.
 * Each translates a negated-errno return into the -1/errno convention (__ret).
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include "syscall_internal.h"

ssize_t read(int fd, void *buf, size_t n)
{
	return (ssize_t)__ret(__sys3(SYS_read, fd, buf, n));
}

ssize_t write(int fd, const void *buf, size_t n)
{
	return (ssize_t)__ret(__sys3(SYS_write, fd, buf, n));
}

int close(int fd)
{
	return (int)__ret(__sys1(SYS_close, fd));
}

off_t lseek(int fd, off_t off, int whence)
{
	return (off_t)__ret(__sys3(SYS_lseek, fd, off, whence));
}

/* open() maps to openat(AT_FDCWD, ...). The optional mode arg is only used with
 * O_CREAT; we read it unconditionally as a long (varargs-free: our kernel takes
 * mode in a4, and passing a stray value is harmless when O_CREAT is unset). */
int openat(int dirfd, const char *path, int flags, int mode)
{
	return (int)__ret(__syscall6(SYS_openat, dirfd, (long)path, flags, mode, 0, 0));
}

/* mount(source, target, fstype, flags, data) — 5 args via __sys6 (+trailing 0).
 * On the custom kernel SYS_mount currently succeeds as a no-op (no procfs/sysfs/
 * devtmpfs backend yet); on mainline it really mounts. Lets /init + a `mount`
 * coreutil issue the standard proc/sys/dev mounts portably. */
int mount(const char *source, const char *target, const char *fstype,
          unsigned long flags, const void *data)
{
	return (int)__ret(__sys6(SYS_mount, source, target, fstype, flags, data, 0));
}

int umount2(const char *target, int flags)
{
	return (int)__ret(__sys2(SYS_umount2, target, flags));
}

int umount(const char *target)
{
	return umount2(target, 0);
}

int open(const char *path, int flags, ...)
{
	int mode = 0;
	if (flags & O_CREAT) {
		__builtin_va_list ap;
		__builtin_va_start(ap, flags);
		mode = __builtin_va_arg(ap, int);
		__builtin_va_end(ap);
	}
	return openat(AT_FDCWD, path, flags, mode);
}

pid_t fork(void)
{
	return (pid_t)__ret(__sys0(SYS_fork));
}

int execve(const char *path, char *const argv[], char *const envp[])
{
	return (int)__ret(__sys3(SYS_execve, path, argv, envp));
}

int execv(const char *path, char *const argv[])
{
	extern char **environ;
	return execve(path, argv, environ);
}

pid_t getpid(void)
{
	return (pid_t)__sys0(SYS_getpid);   /* never fails */
}

void _exit(int status)
{
	__sys1(SYS_exit_group, status);
	__sys1(SYS_exit, status);           /* fallback if exit_group is a no-op */
	for (;;) { }
}

int chdir(const char *path)
{
	return (int)__ret(__sys1(SYS_chdir, path));
}

char *getcwd(char *buf, size_t size)
{
	long r = __ret(__sys2(SYS_getcwd, buf, size));
	return (r < 0) ? NULL : buf;
}
