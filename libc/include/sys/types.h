/*
 * sys/types.h — POSIX-ish typedefs (subset), ARM32 sizes.
 */
#ifndef _GV3_SYS_TYPES_H
#define _GV3_SYS_TYPES_H

#include <stddef.h>

typedef int            pid_t;
typedef int            mode_t;
typedef long           off_t;      /* 32-bit off_t (lseek); large files via _llseek later */
typedef long           time_t;
typedef unsigned int   uid_t;
typedef unsigned int   gid_t;

#endif /* _GV3_SYS_TYPES_H */
