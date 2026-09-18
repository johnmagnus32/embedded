/*
 * epoll.c — epoll_create1 / epoll_ctl / epoll_wait wrappers.
 */
#include <sys/epoll.h>
#include "syscall_internal.h"

int epoll_create1(int flags)
{
	return (int)__ret(__sys1(SYS_epoll_create1, flags));
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
	return (int)__ret(__syscall6(SYS_epoll_ctl, epfd, op, fd, (long)ev, 0, 0));
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	return (int)__ret(__syscall6(SYS_epoll_wait, epfd, (long)events, maxevents, timeout, 0, 0));
}
