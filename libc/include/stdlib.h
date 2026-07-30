/*
 * stdlib.h — allocation + a couple of conversions (subset).
 */
#ifndef _GV3_STDLIB_H
#define _GV3_STDLIB_H

#include <stddef.h>

void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *p, size_t n);

void  exit(int status) __attribute__((noreturn));
void  abort(void) __attribute__((noreturn));

int   atoi(const char *s);

#endif /* _GV3_STDLIB_H */
