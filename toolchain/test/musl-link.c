/* musl-link.c — a real-libc program for the musl link gate: printf (%llu / %f), qsort, malloc, strtod and errno
 * all come from musl's libc.a; 64-bit division from the compiler runtime. The last line has no newline, so it only
 * appears if exit() flushes stdout — i.e. if musl's weak __stdout_used alias lost to stdout.o's strong one. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
int main(void)
{
	int v[] = { 5, 3, 9, 1, 7 };
	qsort(v, 5, sizeof *v, cmp);
	char *p = malloc(64);
	snprintf(p, 64, "%d%d%d%d%d", v[0], v[1], v[2], v[3], v[4]);
	volatile unsigned long long big = 1000000000123ULL;
	double d = strtod("2.5", 0);
	errno = 0; strtol("99999999999999999999", 0, 10);
	printf("musl-link %s %llu %.3f %s\n", p, big / 7, d * 3, errno == ERANGE ? "erange" : "noerr");
	printf("musl-link flushed-at-exit");         /* no newline: only exit()'s stdio flush prints it (needs the real __stdout_used) */
	return 0;
}
