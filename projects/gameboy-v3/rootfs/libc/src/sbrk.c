/*
 * sbrk.c — sbrk() over the kernel's brk(2). Our kernel's sys_brk returns the
 * RESULTING break (not -errno): brk(0) queries, brk(new) grows/shrinks and
 * returns the new break on success or the OLD break on failure. sbrk() adapts
 * that to the classic "return the PREVIOUS break, or (void*)-1 on failure".
 */
#include <unistd.h>
#include <errno.h>
#include "syscall_internal.h"

static unsigned long cur_brk;    /* cached current break (0 = not yet queried) */

void *sbrk(long incr)
{
	if (cur_brk == 0)
		cur_brk = (unsigned long)__sys1(SYS_brk, 0);   /* query initial break */

	unsigned long old = cur_brk;
	if (incr == 0)
		return (void *)old;

	unsigned long want = old + (unsigned long)incr;
	unsigned long got  = (unsigned long)__sys1(SYS_brk, want);
	if (got < want) {                 /* kernel refused (returned old/less) */
		errno = ENOMEM;
		return (void *)-1;
	}
	cur_brk = got;
	return (void *)old;               /* POSIX: sbrk returns the PREVIOUS break */
}
