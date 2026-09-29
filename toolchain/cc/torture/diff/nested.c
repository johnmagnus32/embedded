/* nested.c — GNU nested functions: enclosing locals of every kind (narrow, 64-bit, double, struct, array, VLA and its
 * sizeof, stack-passed parameters) read and written 1-3 levels down; calls from the definer, from itself, from a
 * sibling and from deeper nested ones; results of every class (struct, double, float under VFP, 64-bit); a variadic
 * one; non-local goto out of deep recursion and between levels; && of an enclosing __label__; over-aligned locals.
 * Must print what GCC's build prints. */
int printf(const char *, ...);
struct P { int a; short b; char c; };
static int outer(int p0, int p1, int p2, int p3, int p4, double d5, int n) {
	char c = 1; short s = 2; long long ll = 0x100000000LL; double d = 1.5; struct P st = { 3, 4, 5 }; int arr[4] = { 6, 7, 8, 9 };
	int vla[n]; for (int i = 0; i < n; i++) vla[i] = i * i;
	int level1(int k) {
		int mine = k * 10;
		int level2(int j) {
			int level3(void) { c += 1; ll += mine; return (int)sizeof vla + vla[n - 1] + p4 + (int)d5 + st.c + mine; }
			s = (short)(s + j); return level3() + level1(0 /* recursion into the parent's sibling */ * 0 + (j > 100));
		}
		if (k == 0) return st.a + arr[3];
		arr[k & 3] += k; d *= 2; return level2(k + 1) + mine;
	}
	struct P mk(int x) { struct P r = { x, (short)s, c }; return r; }
	double half(double x) { return x / 2 + d; }
	float fsum(float a, float b) { return a + b + (float)p0; }
	long long wide(void) { return ll + p1; }
	int sum(int cnt, ...) { __builtin_va_list ap; __builtin_va_start(ap, cnt); int t = p2; while (cnt--) t += __builtin_va_arg(ap, int); __builtin_va_end(ap); return t; }
	int r = level1(3);
	struct P q = mk(p3);
	printf("outer r=%d c=%d s=%d ll=%llx d=%d arr=%d,%d,%d,%d q=%d,%d,%d half=%d fsum=%d wide=%llx sum=%d\n", r, c, s, ll, (int)(d * 4),
	       arr[0], arr[1], arr[2], arr[3], q.a, q.b, q.c, (int)(half(9) * 4), (int)(fsum(1.5f, 2.25f) * 4), wide(), sum(3, 10, 20, 30));
	return r;
}
static int unwind(int depth) {
	__label__ out, other;
	int steps = 0;
	void down(int k) {
		void deeper(int m) { if (m == 0) goto out; steps++; if (m == -5) goto other; down(m - 1); }
		steps++; deeper(k);
	}
	void *pick(int w) { return w ? &&other : &&out; }
	down(depth);
	return -1;
out:
	printf("unwind %d steps=%d\n", depth, steps);
	if (depth == 7) goto *pick(1);
	return steps;
other:
	printf("other %d\n", steps);
	return steps + 1000;
}
static int same_name(int x) { int f(void) { return x + 1; } return f(); }
static int same_name2(int x) { int f(void) { return x + 2; } int g(void) { return f() * 10; } return g(); }
typedef int a8 __attribute__((aligned(8)));
static void aligned(void) {
	char pad = 0; a8 v8 = 8; int v16 __attribute__((aligned(16))) = 16; char v64[3] __attribute__((aligned(64))) = { 64 };
	void set(void) { v8++; v16++; v64[0]++; pad++; }
	set();
	printf("aligned %d %d %d %d | %d %d %d\n", v8, v16, v64[0], pad, (int)((unsigned)&v8 & 7), (int)((unsigned)&v16 & 15), (int)((unsigned)v64 & 63));
}
int main(void) {
	outer(1, 2, 3, 4, 5, 6.5, 3);
	printf("u %d %d %d\n", unwind(0), unwind(5), unwind(7));
	printf("names %d %d\n", same_name(1), same_name2(1));
	aligned();
	return 0;
}
