/*
 * stdlib.c — exit/abort/atoi. exit() is _exit() for now (no atexit handlers,
 * no stdio flush yet since our stdio is unbuffered).
 */
#include <stdlib.h>
#include <unistd.h>

void exit(int status)
{
	_exit(status);
}

void abort(void)
{
	_exit(127);
}

int atoi(const char *s)
{
	int sign = 1, v = 0;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == '-') { sign = -1; s++; }
	else if (*s == '+') s++;
	while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
	return sign * v;
}
