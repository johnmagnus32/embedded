/*
 * stdio.h — a MINIMAL stdio: just enough formatted output for our programs.
 * Unbuffered (each call write()s directly), no FILE* streams yet — printf goes
 * to stdout, fprintf takes an fd-like int. Format support: %s %c %d %u %x %p %%.
 */
#ifndef _GV3_STDIO_H
#define _GV3_STDIO_H

#include <stddef.h>

int printf(const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);   /* fd-targeted printf */
int puts(const char *s);                      /* writes s + '\n' to stdout */
int putchar(int c);

#endif /* _GV3_STDIO_H */
