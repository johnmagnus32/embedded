/*
 * sys/un.h — AF_UNIX address.
 */
#ifndef _LIBC_SYS_UN_H
#define _LIBC_SYS_UN_H

#include <sys/socket.h>

struct sockaddr_un {
	sa_family_t sun_family;
	char        sun_path[108];
};

#endif /* _LIBC_SYS_UN_H */
