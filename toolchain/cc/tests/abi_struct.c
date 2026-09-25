// expect: 63
/* AAPCS composite passing/returning, checked against GCC-built code's view of the ABI:
 * by-value structs in argument words (split across r3 + stack), sret (> 4 bytes) and r0 (<= 4 bytes)
 * returns, 8-byte-aligned composites, va_arg of a struct, and `*p` / `a[i]` of struct type as rvalues. */
typedef __builtin_va_list va_list;   /* cc tests are not preprocessed: builtins spelled out */
struct big { char a[32]; };
struct tiny { int c; };
struct three { char a, b, c; };
struct al8 { long long v; };
static int f_big(struct big x, char *s, ...) {        /* x = r0-r3 + 16 stack bytes; s + varargs follow on the stack */
	va_list ap; __builtin_va_start(ap, s);
	int a = __builtin_va_arg(ap, int), b = __builtin_va_arg(ap, int);
	__builtin_va_end(ap);
	return x.a[0] == 'a' && x.a[31] == 'z' && *s == 'q' && a == 42 && b == 'x';
}
static int f_split(int a, int b, int c, struct tiny t, struct big x) { return a + b + c == 6 && t.c == 7 && x.a[20] == 'm'; }   /* x splits r... none left: stack */
static int f_al8(int a, struct al8 s) { return a == 1 && s.v == 0x100000002LL; }   /* s even-aligned: r2:r3, r1 skipped */
static struct big mk_big(char c) { struct big b; for (int i = 0; i < 32; i++) b.a[i] = c + i; return b; }   /* sret */
static struct three mk_three(void) { struct three t = { 1, 2, 3 }; return t; }                          /* in r0 */
static struct tiny mk_tiny(int v) { struct tiny t; t.c = v; return t; }
static int f_va(int n, ...) {
	va_list ap; __builtin_va_start(ap, n); int ok = 1;
	for (int i = 0; i < n; i++) { struct tiny x = __builtin_va_arg(ap, struct tiny); ok &= x.c == i + 10; }
	struct al8 z = __builtin_va_arg(ap, struct al8); ok &= z.v == 5;
	__builtin_va_end(ap); return ok;
}
int main(void) {
	static struct big b = { "abc" }; b.a[0] = 'a'; b.a[31] = 'z'; b.a[20] = 'm';
	struct tiny t[3] = { {10}, {11}, {12} }, *tp = &t[1];
	struct al8 s8 = { 0x100000002LL }, five = { 5 };
	int r = 0;
	r += f_big(b, "q", 42, 'x');                              /* 1 */
	r += f_split(1, 2, 3, (struct tiny){7}, b) << 1;          /* 2 */
	r += f_al8(1, s8) << 2;                                   /* 4 */
	struct big m = mk_big('A'); r += (m.a[0] == 'A' && m.a[31] == 'A' + 31) << 3;   /* 8 */
	r += (mk_three().c == 3 && mk_tiny(9).c == 9) << 4;       /* 16 */
	r += f_va(3, t[0], *tp, t[2], five) << 5;                 /* 32: t[i] and *p are struct rvalues */
	return r;
}
