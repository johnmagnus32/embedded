/*
 * stdlib.c — exit/abort, integer conversions, environment, qsort, rand.
 * exit() is _exit() (no atexit handlers; our stdio output is unbuffered so nothing to flush).
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

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

static int digit_val(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'z') return c - 'a' + 10;
	if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
	return 99;
}

long strtol(const char *s, char **end, int base)
{
	const char *start = s;
	int neg = 0, any = 0, ovf = 0;
	while (*s == ' ' || *s == '\t' || *s == '\n') s++;
	if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
	/* consume a "0x" prefix only when a hex digit actually follows */
	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && digit_val(s[2]) < 16) {
		s += 2; base = 16;
	} else if (base == 0 && s[0] == '0') base = 8;
	else if (base == 0) base = 10;

	/* accumulate unsigned; clamp to LONG_MAX/LONG_MIN on overflow (no signed UB) */
	unsigned long limit = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
	unsigned long cut = limit / (unsigned long)base;
	int cutd = (int)(limit % (unsigned long)base);
	unsigned long acc = 0;
	for (int d; (d = digit_val(*s)) < base; s++) {
		any = 1;
		if (ovf || acc > cut || (acc == cut && d > cutd)) ovf = 1;
		else acc = acc * (unsigned long)base + (unsigned long)d;
	}
	if (end) *end = (char *)(any ? s : start);
	if (ovf) { errno = ERANGE; return neg ? LONG_MIN : LONG_MAX; }
	return neg ? -(long)acc : (long)acc;
}

/* ---- environment (over the global `environ`) ------------------------------ */
extern char **environ;
static char **env_heap;   /* our malloc'd environ once we grow/modify it */
static size_t env_cap;

static size_t env_count(void)
{
	size_t n = 0;
	if (environ) while (environ[n]) n++;
	return n;
}

/* Ensure environ is heap-backed with room for `extra` more entries + the NULL. */
static int env_ensure(size_t extra)
{
	size_t n = env_count();
	if (environ == env_heap && n + extra + 1 <= env_cap) return 0;
	size_t cap = n + extra + 8;
	char **p = malloc(cap * sizeof *p);
	if (!p) return -1;
	for (size_t i = 0; i < n; i++) p[i] = environ[i];
	p[n] = NULL;
	env_heap = environ = p;
	env_cap = cap;
	return 0;
}

char *getenv(const char *name)
{
	size_t l = strlen(name);
	if (!environ) return NULL;
	for (char **e = environ; *e; e++)
		if (!strncmp(*e, name, l) && (*e)[l] == '=') return *e + l + 1;
	return NULL;
}

/* putenv: install `s` ("KEY=VALUE") into environ, replacing any entry with the same key.
 * Stores the pointer directly (POSIX). */
int putenv(char *s)
{
	const char *eq = strchr(s, '=');
	size_t l = eq ? (size_t)(eq - s) : strlen(s);
	size_t n = env_count();
	for (size_t i = 0; i < n; i++)
		if (!strncmp(environ[i], s, l) && environ[i][l] == '=') { environ[i] = s; return 0; }
	if (env_ensure(1)) return -1;
	n = env_count();
	environ[n] = s;
	environ[n + 1] = NULL;
	return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite && getenv(name)) return 0;
	size_t ln = strlen(name), lv = strlen(value);
	char *s = malloc(ln + lv + 2);
	if (!s) return -1;
	memcpy(s, name, ln);
	s[ln] = '=';
	memcpy(s + ln + 1, value, lv);
	s[ln + lv + 1] = '\0';
	return putenv(s);   /* replaces or appends; an old malloc'd value string is leaked (rare) */
}

int unsetenv(const char *name)
{
	size_t l = strlen(name);
	if (!environ) return 0;
	size_t i = 0;
	while (environ[i]) {
		if (!strncmp(environ[i], name, l) && environ[i][l] == '=') {
			/* shift the (NULL-terminated) tail down over slot i; reads environ[j+1] only
			 * while environ[j] is non-NULL, so it never runs past the terminator. */
			for (size_t j = i; environ[j]; j++) environ[j] = environ[j + 1];
		} else {
			i++;
		}
	}
	return 0;
}

/* ---- qsort (insertion sort: O(n^2), fine for our small arrays) ------------- */
static void swap_bytes(char *a, char *b, size_t size)
{
	for (size_t i = 0; i < size; i++) { char t = a[i]; a[i] = b[i]; b[i] = t; }
}

void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *))
{
	char *a = base;
	for (size_t i = 1; i < nmemb; i++)
		for (size_t j = i; j > 0 && cmp(a + (j - 1) * size, a + j * size) > 0; j--)
			swap_bytes(a + (j - 1) * size, a + j * size, size);
}

/* ---- rand (LCG; deterministic, adequate for games) ------------------------ */
static unsigned long rand_state = 1;
void srand(unsigned seed) { rand_state = seed; }
int rand(void)
{
	rand_state = rand_state * 1103515245UL + 12345UL;
	return (int)((rand_state >> 16) & RAND_MAX);
}
