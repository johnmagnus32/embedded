/*
 * timerfd.c — timerfd_create / timerfd_settime wrappers.
 */
#include <sys/timerfd.h>
#include "syscall_internal.h"

int timerfd_create(int clockid, int flags)
{
	return (int)__ret(__sys2(SYS_timerfd_create, clockid, flags));
}

int timerfd_settime(int fd, int flags, const struct itimerspec *new_value,
                    struct itimerspec *old_value)
{
	return (int)__ret(__syscall6(SYS_timerfd_settime, fd, flags,
	                             (long)new_value, (long)old_value, 0, 0));
}
