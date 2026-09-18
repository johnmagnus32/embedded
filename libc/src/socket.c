/*
 * socket.c — socket wrappers over the direct ARM socket syscalls (socket=281..).
 */
#include <sys/socket.h>
#include "syscall_internal.h"

int socket(int domain, int type, int protocol)
{
	return (int)__ret(__sys3(SYS_socket, domain, type, protocol));
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return (int)__ret(__sys3(SYS_bind, fd, addr, len));
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	return (int)__ret(__sys3(SYS_connect, fd, addr, len));
}

int listen(int fd, int backlog)
{
	return (int)__ret(__sys2(SYS_listen, fd, backlog));
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	return (int)__ret(__sys3(SYS_accept, fd, addr, len));
}

int accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags)
{
	return (int)__ret(__syscall6(SYS_accept4, fd, (long)addr, (long)len, flags, 0, 0));
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	return (int)__ret(__syscall6(SYS_setsockopt, fd, level, optname, (long)optval, optlen, 0));
}

long sendmsg(int fd, const struct msghdr *msg, int flags)
{
	return (long)__ret(__sys3(SYS_sendmsg, fd, msg, flags));
}

long recvmsg(int fd, struct msghdr *msg, int flags)
{
	return (long)__ret(__sys3(SYS_recvmsg, fd, msg, flags));
}

struct cmsghdr *__cmsg_nxthdr(struct msghdr *m, struct cmsghdr *c)
{
	unsigned char *next = (unsigned char *)c + CMSG_ALIGN(c->cmsg_len);
	unsigned char *end  = (unsigned char *)m->msg_control + m->msg_controllen;
	if (next + sizeof(struct cmsghdr) > end) return (struct cmsghdr *)0;
	return (struct cmsghdr *)next;
}
