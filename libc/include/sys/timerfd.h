/*
 * sys/timerfd.h — a timer that fires by making an fd readable (u64 expiration count).
 */
#ifndef _LIBC_SYS_TIMERFD_H
#define _LIBC_SYS_TIMERFD_H

#include <time.h>   /* struct itimerspec, CLOCK_* */

#define TFD_CLOEXEC       02000000
#define TFD_NONBLOCK      04000
#define TFD_TIMER_ABSTIME 1

int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value,
                    struct itimerspec *old_value);

#endif /* _LIBC_SYS_TIMERFD_H */
