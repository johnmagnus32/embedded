/*
 * libk.h — prototypes for the freestanding libc primitives in libk.c
 * (the mem and str helpers the kernel and GCC need). Tiny, self-contained.
 */
#ifndef GV3K_LIBK_H
#define GV3K_LIBK_H

#include <stddef.h>

void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strlcpy_(char *dst, const char *src, size_t cap);

int printf(const char *fmt, ...);

#endif /* GV3K_LIBK_H */
