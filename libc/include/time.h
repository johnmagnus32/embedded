/*
 * time.h — monotonic/realtime clock + sleep (subset). 32-bit time_t timespec matches the
 * ARM SYS_clock_gettime(263)/SYS_nanosleep(162) old_timespec32 layout.
 */
#ifndef _LIBC_TIME_H
#define _LIBC_TIME_H

#include <sys/types.h>

struct timespec {
	time_t tv_sec;
	long   tv_nsec;
};

struct itimerspec {
	struct timespec it_interval;
	struct timespec it_value;
};

typedef int clockid_t;
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

int clock_gettime(clockid_t clk, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);

#endif /* _LIBC_TIME_H */
