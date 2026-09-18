/*
 * time.c — clock_gettime + nanosleep over the ARM 32-bit-timespec syscalls.
 */
#include <time.h>
#include "syscall_internal.h"

int clock_gettime(clockid_t clk, struct timespec *ts)
{
	return (int)__ret(__sys2(SYS_clock_gettime, clk, ts));
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return (int)__ret(__sys2(SYS_nanosleep, req, rem));
}
