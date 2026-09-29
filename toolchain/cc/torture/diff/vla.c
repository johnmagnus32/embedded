/* vla.c — structs with variable-length members (GNU): runtime sizeof and member offsets (alignment after the VLA,
 * packed bit-fields after it, aligned members), arrays of them and pointer strides, offsetof with runtime parts,
 * copies, va_arg of one (passed by reference), a nested function using them, the size frozen at the declaration.
 * Must match GCC. */
int printf(const char *, ...);
static void show(int n) {
	struct A { char c; short v[n]; int after; char tail; } a;
	struct __attribute__((packed)) B { int i[n]; unsigned b1 : 1, b2 : 3; int x; char y; } b;
	struct C { __attribute__((aligned(16))) struct { short s; } e[n]; long long z[n]; int w; } c;
	printf("n=%d A %d %d %d | B %d %d | C %d %d %d %d\n", n, (int)sizeof a, (int)__builtin_offsetof(struct A, after), (int)__builtin_offsetof(struct A, tail),
	       (int)sizeof b, (int)__builtin_offsetof(struct B, x), (int)sizeof c, (int)__builtin_offsetof(struct C, z), (int)__builtin_offsetof(struct C, w),
	       (int)(((unsigned)&c) & 15));
	a.c = 1; for (int i = 0; i < n; i++) a.v[i] = (short)(i * 3); a.after = 77; a.tail = 9;
	b.b1 = 1; b.b2 = 5; b.x = -4; b.y = 3; for (int i = 0; i < n; i++) b.i[i] = i;
	printf("A %d %d %d %d | B %d %d %d %d %d\n", a.c, a.v[n - 1], a.after, a.tail, b.b1, b.b2, b.x, b.y, b.i[n - 1]);
	struct A arr[3]; arr[2].after = 5; arr[1] = arr[2]; struct A *p = arr;
	printf("arr %d %d %d\n", (int)sizeof arr, p[1].after, (int)((char *)&p[2] - (char *)p));
	int k = 2; printf("offsetof %d %d\n", (int)__builtin_offsetof(struct A, v[k]), (int)__builtin_offsetof(struct C, z[k]));
	int bump(void) { return a.after + b.x + (int)sizeof(struct A); }   /* the enclosing function's variable-size layout */
	printf("nested %d\n", bump());
}
static int va(int n, ...) {
	struct D { char s[n]; } d;
	__builtin_va_list ap; __builtin_va_start(ap, n);
	d = __builtin_va_arg(ap, struct D); int t = __builtin_va_arg(ap, int);
	__builtin_va_end(ap);
	return d.s[0] * 100 + d.s[n - 1] + t * 1000;
}
static int frozen(int n) { struct F { char b[n]; }; n++; return sizeof(struct F) * 10 + n; }
int main(void) {
	show(1); show(3); show(4);
	int m = 3; struct { char s[m]; } x; x.s[0] = 7; x.s[2] = 9;
	printf("va %d frozen %d\n", va(m, x, 5), frozen(12));
	return 0;
}
