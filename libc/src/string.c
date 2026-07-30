/*
 * string.c — freestanding mem/str primitives. Ported from the kernel's libk.c;
 * these are also the functions GCC may emit calls to (memcpy/memset), so they
 * must exist even in -ffreestanding builds.
 */
#include <string.h>

void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) *d++ = *s++;
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

void *memset(void *dst, int c, size_t n)
{
	unsigned char *d = dst;
	while (n--) *d++ = (unsigned char)c;
	return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *x = a, *y = b;
	while (n--) { if (*x != *y) return *x - *y; x++; y++; }
	return 0;
}

size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n]) n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	while (n && *a && *a == *b) { a++; b++; n--; }
	if (n == 0) return 0;
	return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src)
{
	char *d = dst;
	while ((*d++ = *src++)) { }
	return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i]; i++) dst[i] = src[i];
	for (; i < n; i++) dst[i] = '\0';
	return dst;
}

char *strchr(const char *s, int c)
{
	for (; *s; s++) if (*s == (char)c) return (char *)s;
	return (c == '\0') ? (char *)s : NULL;
}
