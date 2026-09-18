/*
 * signalfd.c — signalfd() over signalfd4(fd, mask, sizemask, flags). sizemask = 8 (the
 * 64-bit sigset the kernel's rt_* ABI expects).
 */
#include <sys/signalfd.h>
#include "syscall_internal.h"

int signalfd(int fd, const sigset_t *mask, int flags)
{
	return (int)__ret(__syscall6(SYS_signalfd4, fd, (long)mask, 8, flags, 0, 0));
}
