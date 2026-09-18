/*
 * sys/socket.h — sockets (subset), direct ARM socket syscalls (not socketcall).
 * struct iovec + the CMSG_* ancillary-data helpers live here (no separate sys/uio.h);
 * they back SCM_RIGHTS fd passing over AF_UNIX (used by the canvas SDK).
 */
#ifndef _LIBC_SYS_SOCKET_H
#define _LIBC_SYS_SOCKET_H

#include <sys/types.h>

typedef unsigned int   socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr { sa_family_t sa_family; char sa_data[14]; };

struct iovec { void *iov_base; size_t iov_len; };

struct msghdr {
	void         *msg_name;
	socklen_t     msg_namelen;
	struct iovec *msg_iov;
	size_t        msg_iovlen;
	void         *msg_control;
	size_t        msg_controllen;
	int           msg_flags;
};

struct cmsghdr {
	size_t cmsg_len;
	int    cmsg_level;
	int    cmsg_type;
	/* followed by cmsg_len - sizeof(struct cmsghdr) bytes of data */
};

#define AF_UNIX   1
#define AF_LOCAL  1
#define PF_UNIX   1
#define AF_INET   2
#define PF_INET   2

#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC   02000000
#define SOCK_NONBLOCK  04000

#define SOL_SOCKET     1
#define SO_REUSEADDR   2
#define SCM_RIGHTS     1

/* ancillary-data (control message) helpers */
#define __CMSG_ALIGN(n) (((n) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_ALIGN(n)   __CMSG_ALIGN(n)
#define CMSG_LEN(n)     (__CMSG_ALIGN(sizeof(struct cmsghdr)) + (n))
#define CMSG_SPACE(n)   (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(n))
#define CMSG_DATA(c)    ((unsigned char *)((struct cmsghdr *)(c) + 1))
#define CMSG_FIRSTHDR(m) ((m)->msg_controllen >= sizeof(struct cmsghdr) \
                          ? (struct cmsghdr *)(m)->msg_control : (struct cmsghdr *)0)
#define CMSG_NXTHDR(m, c) __cmsg_nxthdr((m), (c))
struct cmsghdr *__cmsg_nxthdr(struct msghdr *m, struct cmsghdr *c);

int  socket(int domain, int type, int protocol);
int  bind(int fd, const struct sockaddr *addr, socklen_t len);
int  connect(int fd, const struct sockaddr *addr, socklen_t len);
int  listen(int fd, int backlog);
int  accept(int fd, struct sockaddr *addr, socklen_t *len);
int  accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags);
int  setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);
long sendmsg(int fd, const struct msghdr *msg, int flags);
long recvmsg(int fd, struct msghdr *msg, int flags);

#endif /* _LIBC_SYS_SOCKET_H */
