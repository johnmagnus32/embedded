/*
 * tty.c — isatty() over ioctl(TIOCGWINSZ), exactly how musl probes for a tty.
 * Our kernel answers TIOCGWINSZ (0x5413) with success only on /dev/console, so
 * isatty(0/1) is true on the console and false for files/pipes.
 */
#include <unistd.h>
#include "syscall_internal.h"

#define TIOCGWINSZ 0x5413

int isatty(int fd)
{
	unsigned short ws[4];
	long r = __sys3(SYS_ioctl, fd, TIOCGWINSZ, (long)ws);
	return (r == 0) ? 1 : 0;
}
