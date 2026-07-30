/*
 * libk.c — the handful of libc primitives GCC emits calls to even under
 * -ffreestanding. Struct assignment (`p->tf = *tf`) and array initialization
 * lower to memcpy/memset, so we must provide them ourselves (no libc linked).
 * Simple byte-wise implementations — correctness over speed.
 */

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) *d++ = *s++;
	return dst;
}

void *memset(void *dst, int c, size_t n)
{
	unsigned char *d = dst;
	while (n--) *d++ = (unsigned char)c;
	return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	if (d < s) {
		while (n--) *d++ = *s++;
	} else {
		d += n; s += n;
		while (n--) *--d = *--s;
	}
	return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *p = a, *q = b;
	while (n--) { if (*p != *q) return (int)*p - (int)*q; p++; q++; }
	return 0;
}

/* ---- small string helpers used by the ramfs/cpio/VFS (S9) --------------- */
size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n]) n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	while (n && *a && *a == *b) { a++; b++; n--; }
	if (n == 0) return 0;
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src)
{
	char *d = dst;
	while ((*d++ = *src++)) ;
	return dst;
}

/* Bounded copy that always NUL-terminates (unlike strncpy). Returns dst. */
char *strlcpy_(char *dst, const char *src, size_t cap)
{
	size_t i = 0;
	if (cap == 0) return dst;
	for (; i < cap - 1 && src[i]; i++) dst[i] = src[i];
	dst[i] = '\0';
	return dst;
}
