/* constexpr.c — what is an integer CONSTANT EXPRESSION, as GCC decides it, when our folding makes more things fold:
 * the kernel's __is_constexpr(x) (x * 0 and 0 && x fold but aren't ICEs), a read-only local and an inlined function's
 * constant result (they fold, but `char a[n]` stays a VLA), and a _Bool conversion of a constant (!= 0). */
int printf(const char *, ...);
#define __is_constexpr(x) (sizeof(int) == sizeof(*(8 ? ((void *)((long)(x) * 0l)) : (int *)8)))
int v = 3;
static inline int four(void) { return 4; }
static inline int twice(int x) { return 2 * x; }
int main(void) {
	int n = 5;
	char a[n]; char b[twice(3)];
	struct { char c[n]; } s;
	_Bool t = 0x100, u = (_Bool)0x200;
	printf("%d %d %d %d %d %d\n", (int)__is_constexpr(128), (int)__is_constexpr(v), (int)__is_constexpr(v * 0),
	       (int)__is_constexpr(0 && v), (int)__is_constexpr(n), (int)__is_constexpr(four()));
	printf("%d %d %d %d %d\n", (int)sizeof a, (int)sizeof b, (int)sizeof s, t, u);
	return 0;
}
