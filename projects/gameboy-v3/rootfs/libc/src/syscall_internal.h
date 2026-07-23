/*
 * syscall_internal.h — private to gv3libc: the raw svc entry + errno plumbing.
 * Not installed as a public header (programs use unistd.h/fcntl.h/etc).
 */
#ifndef _GV3_SYSCALL_INTERNAL_H
#define _GV3_SYSCALL_INTERNAL_H

#include <sys/syscall.h>
#include <errno.h>

/* The two raw entry points (syscall.c). Everything else is built on these. */
long __syscall3(long nr, long a0, long a1, long a2);
long __syscall6(long nr, long a0, long a1, long a2, long a3, long a4, long a5);

/* Convenience: most syscalls need <=3 args. */
#define __sys0(nr)                 __syscall3((nr), 0, 0, 0)
#define __sys1(nr, a)              __syscall3((nr), (long)(a), 0, 0)
#define __sys2(nr, a, b)           __syscall3((nr), (long)(a), (long)(b), 0)
#define __sys3(nr, a, b, c)        __syscall3((nr), (long)(a), (long)(b), (long)(c))
#define __sys6(nr, a, b, c, d, e, f) \
	__syscall6((nr), (long)(a), (long)(b), (long)(c), (long)(d), (long)(e), (long)(f))

/*
 * __ret — turn a raw syscall return into the POSIX (-1 + errno) convention.
 * A value in [-4095, -1] is a negated errno; anything else is a real result.
 * Callers that want the raw value (e.g. mmap, which uses MAP_FAILED, or brk,
 * whose contract returns the break) skip this and check ranges themselves.
 */
static inline long __ret(long r)
{
	if (r < 0 && r > -4096) {
		errno = (int)(-r);
		return -1;
	}
	return r;
}

#endif /* _GV3_SYSCALL_INTERNAL_H */
