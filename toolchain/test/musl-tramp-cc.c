/* musl-tramp-cc.c — GNU nested functions called through their ADDRESS, compiled by OUR cc: each address is a
 * trampoline written to the definer's stack frame (so it needs __clear_cache's cacheflush syscall and an executable
 * stack — a real Linux, not the semihosting harness). Covers: musl's qsort calling one back, a trampoline outliving
 * later changes to the locals it reaches, recursion through its own address, an address taken two levels down (in a
 * nested function of the definer), and a definer that is itself nested. */
typedef unsigned long size_t;
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
int printf(const char *fmt, ...);
static int apply(int (*f)(int), int x) { return f(x); }
static int sorted(int desc)
{
	int calls = 0, a[6] = { 5, 3, 9, 1, 7, 2 };
	int cmp(const void *p, const void *q) { calls++; int d = *(const int *)p - *(const int *)q; return desc ? -d : d; }
	qsort(a, 6, sizeof a[0], cmp);
	return a[0] * 100000 + a[1] * 10000 + a[5] * 10 + (calls > 0);
}
static int outer(int base)
{
	int k = base;
	int add(int x) { return x + k; }
	int (*fp)(int) = add;
	k += 10;                                       /* the trampoline reaches the frame, not a copy */
	int fact(int n) { return n <= 1 ? 1 : n * apply(fact, n - 1); }
	int deep(int x) { int twice(int y) { return add(add(y)); } return apply(twice, x) + apply(add, 0); }
	return fp(1) * 10000 + apply(fact, 5) * 10 + deep(0) % 10;
}
static int nested_definer(void)
{
	int v = 3;
	int mid(int x) { int m = x; int inner(int y) { return y * m + v; } return apply(inner, 4); }
	return apply(mid, 5);                          /* 4*5 + 3 */
}
int main(void)
{
	printf("musl-tramp %d %d %d %d\n", sorted(0), sorted(1), outer(2), nested_definer());
	return 0;
}
