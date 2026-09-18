/*
 * stdlib.h — allocation + a couple of conversions (subset).
 */
#ifndef _LIBC_STDLIB_H
#define _LIBC_STDLIB_H

#include <stddef.h>

void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *p, size_t n);

void  exit(int status) __attribute__((noreturn));
void  abort(void) __attribute__((noreturn));

int   atoi(const char *s);
long  strtol(const char *s, char **end, int base);

char *getenv(const char *name);
int   setenv(const char *name, const char *value, int overwrite);
int   putenv(char *string);
int   unsetenv(const char *name);

void  qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *));

int   rand(void);
void  srand(unsigned seed);
#define RAND_MAX 0x7fffffff

#endif /* _LIBC_STDLIB_H */
