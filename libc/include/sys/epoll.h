/*
 * sys/epoll.h — epoll (subset). On ARM struct epoll_event is NOT packed (unlike x86_64),
 * so it is 16 bytes: events(u32) at 0, 4 bytes pad, data(u64) at 8 — matching the kernel.
 */
#ifndef _LIBC_SYS_EPOLL_H
#define _LIBC_SYS_EPOLL_H

#include <stdint.h>

typedef union epoll_data {
	void    *ptr;
	int      fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t     events;
	epoll_data_t data;
};

#define EPOLLIN      0x001
#define EPOLLPRI     0x002
#define EPOLLOUT     0x004
#define EPOLLERR     0x008
#define EPOLLHUP     0x010
#define EPOLLRDHUP   0x2000
#define EPOLLONESHOT 0x40000000
#define EPOLLET      0x80000000

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLL_CLOEXEC 02000000

int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#endif /* _LIBC_SYS_EPOLL_H */
