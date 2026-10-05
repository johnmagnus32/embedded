/* __builtin_assume_aligned: the pointer back (as void *), its hints evaluated only when not constant */
#include <stdio.h>
static int n;
static int side(void) { return ++n * 0 + 8; }
int main(void) {
	int a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	int *p = __builtin_assume_aligned(a, 4);
	char *q = __builtin_assume_aligned((char *)a + 4, 8, 4);
	void *r = __builtin_assume_aligned(&a[2], side());
	printf("%d %d %d %d\n", p[1], *(int *)q, *(int *)r, n);
	return 0;
}
