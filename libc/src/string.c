/*
 * string.c — freestanding mem/str primitives. Ported from the kernel's libk.c;
 * these are also the functions GCC may emit calls to (memcpy/memset), so they
 * must exist even in -ffreestanding builds.
 */
#include <string.h>
#include <stdlib.h>   /* malloc (strdup) */
#include <errno.h>    /* E* codes (strerror) */

void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) *d++ = *s++;
	return dst;
}

void *mempcpy(void *dst, const void *src, size_t n)
{
	return (unsigned char *)memcpy(dst, src, n) + n;
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

char *strcat(char *dst, const char *src)
{
	char *d = dst;
	while (*d) d++;
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

char *strrchr(const char *s, int c)
{
	const char *last = NULL;
	for (;; s++) { if (*s == (char)c) last = s; if (!*s) break; }
	return (char *)last;
}

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	for (; n; n--, p++) if (*p == (unsigned char)c) return (void *)p;
	return NULL;
}

char *strstr(const char *hay, const char *needle)
{
	if (!*needle) return (char *)hay;
	for (; *hay; hay++) {
		const char *h = hay, *n = needle;
		while (*h && *n && *h == *n) { h++; n++; }
		if (!*n) return (char *)hay;
	}
	return NULL;
}

size_t strspn(const char *s, const char *set)
{
	size_t n = 0;
	for (; s[n]; n++) if (!strchr(set, s[n])) break;
	return n;
}

size_t strcspn(const char *s, const char *set)
{
	size_t n = 0;
	for (; s[n]; n++) if (strchr(set, s[n])) break;
	return n;
}

char *strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = malloc(n);
	if (p) memcpy(p, s, n);
	return p;
}

/* Minimal errno -> message table. Unknown numbers get a stable fallback. */
char *strerror(int errnum)
{
	switch (errnum) {
	case 0:       return "Success";
	case EPERM:   return "Operation not permitted";
	case ENOENT:  return "No such file or directory";
	case ESRCH:   return "No such process";
	case EINTR:   return "Interrupted system call";
	case EIO:     return "Input/output error";
	case EBADF:   return "Bad file descriptor";
	case EAGAIN:  return "Resource temporarily unavailable";
	case ENOMEM:  return "Out of memory";
	case EACCES:  return "Permission denied";
	case EFAULT:  return "Bad address";
	case EBUSY:   return "Device or resource busy";
	case EEXIST:  return "File exists";
	case ENOTDIR: return "Not a directory";
	case EISDIR:  return "Is a directory";
	case EINVAL:  return "Invalid argument";
	case ENOSYS:  return "Function not implemented";
	case ERANGE:  return "Numerical result out of range";
	case ECHILD:  return "No child processes";
	case EPIPE:   return "Broken pipe";
	case ENOSPC:  return "No space left on device";
	default:      return "Unknown error";
	}
}
