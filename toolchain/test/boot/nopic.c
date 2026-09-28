/*
 * nopic.c — a dynamically-linked program compiled WITHOUT -fPIC (absolute addressing), booted as PID 1 by
 * boot-dynamic.sh. It exercises the two symbol-binding rules a non-PIC program depends on:
 *   - a library VARIABLE the program uses directly gets a copy in the program (R_ARM_COPY); the program
 *     must export that copy so the library binds to it too — else libc's startup writes its own `environ`
 *     and the program reads a stale copy;
 *   - a library FUNCTION's address taken in the program is its PLT entry (the canonical address), not a
 *     "copy" of the function; libc then calls back through it (qsort with strcmp).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;
static int (*say)(const char *) = puts;                  /* a libc function's address in data (R_ARM_ABS32) */

int main(int argc, char **argv)
{
	char **envp = argv + argc + 1;                       /* the kernel's envp, after argv's NULL */
	char w[4][4] = { "dd", "bb", "aa", "cc" };
	qsort(w, 4, sizeof w[0], (int (*)(const void *, const void *))strcmp);   /* libc calls back through it */
	int (*p)(const char *) = puts;                       /* ...and an address formed in code (movw/movt) */
	if (environ != envp) { say("nopic: environ is a stale copy (libc wrote a different one)"); return 1; }
	printf("nopic: sorted %s %s %s %s\n", w[0], w[1], w[2], w[3]);
	p("nopic: environ shared; libc function pointers work");
	return 0;
}
