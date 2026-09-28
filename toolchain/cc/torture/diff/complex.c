/* complex.c — _Complex (floating and GCC's integer complex types): literals, __real__/__imag__ as lvalues, every
 * operator with complex and real operands on either side (Annex G: a real operand isn't widened), the runtime's
 * __mul?c3/__div?c3 on special values, conversions both ways, op=/++, ?:, initializers (constant folding, members,
 * arrays, elided braces), the ABI (arguments, returns, variadic, a struct holding complex members). Prints the parts'
 * bits (a NaN canonical: its sign is unspecified) — must match GCC. */
int printf(const char *, ...);
typedef unsigned long long u64;
static u64 db(double d) { union { double d; u64 u; } v; v.d = d; return d != d ? 0x7ff8000000000000ULL : v.u; }
static unsigned fb(float f) { union { float f; unsigned u; } v; v.f = f; return f != f ? 0x7fc00000u : v.u; }
#define PD(t, z) printf("%s %016llx %016llx\n", t, db(__real__ (z)), db(__imag__ (z)))
#define PF(t, z) printf("%s %08x %08x\n", t, fb(__real__ (z)), fb(__imag__ (z)))
#define PI(t, z) printf("%s %lld %lld\n", t, (long long)__real__ (z), (long long)__imag__ (z))

_Complex double gd = 1.5 + 2.5i, garr[3] = { 1, 2i, 3 + 4i };
_Complex float gf = 1.0f + 14.0f * (1.0fi);
_Complex int gi = 100 + 200i;
struct S { char c; _Complex float f; _Complex short s; double d; } gs = { 1, 2 + 3i, 4, 5 };
struct E { _Complex double a; int x; } ge = { 7, 8 };   /* 7 -> 7+0i, 8 -> x */
static _Complex double add(_Complex double a, _Complex double b) { return a + b; }
static _Complex float fmix(float s, _Complex float a, int k, _Complex float b) { return a * s + b * k; }
static _Complex long long lmul(_Complex long long a, _Complex long long b) { return a * b; }
static _Complex char cneg(_Complex char a) { return -a; }
static struct S pass(struct S s) { s.f = ~s.f; s.s += 1; return s; }
static _Complex double va(int n, ...) {
	__builtin_va_list ap; __builtin_va_start(ap, n); _Complex double r = 0;
	for (int i = 0; i < n; i++) r += __builtin_va_arg(ap, _Complex double);
	__builtin_va_end(ap); return r;
}
int main(void) {
	volatile double inf = 1.0 / 0.0, nan = 0.0 / 0.0, z = 0.0, one = 1.0;
	_Complex double a = 3 - 4i, b = -1.5 + 0.25i, c;
	PD("g", gd); PD("garr0", garr[0]); PD("garr1", garr[1]); PD("garr2", garr[2]); PF("gf", gf); PI("gi", gi);
	PF("gs.f", gs.f); PI("gs.s", gs.s); printf("gs %d %d %d\n", gs.c, (int)gs.d, (int)sizeof gs); PD("ge", ge.a); printf("ge.x %d\n", ge.x);
	c = a + b; PD("+", c); c = a - b; PD("-", c); c = a * b; PD("*", c); c = a / b; PD("/", c);
	c = a + 2; PD("+r", c); c = 2 - a; PD("r-", c); c = a * 0.5; PD("*r", c); c = 3 / a; PD("r/", c); c = a / 2; PD("/r", c);
	c = -a; PD("neg", c); c = ~a; PD("conj", c); c = __builtin_conj(b); PD("bconj", c);
	printf("== %d %d %d %d\n", a == a, a == b, a != b, (3 - 4i) == a); printf("real %d %d\n", a == 3, 3.0 == (_Complex double)3);
	__real__ c = 9; __imag__ c = -9; PD("parts", c); __real__ c += 1; PD("parts2", c);
	double r = a; printf("toreal %016llx\n", db(r)); int bt = (_Bool)(0 + 0i), bt2 = (_Bool)(0 + 1i); printf("bool %d %d %d\n", bt, bt2, !a);
	if (a) printf("if-true\n"); if (!(0.0 + 0.0i)) printf("if-zero\n");
	_Complex float f = a; PF("tofloat", f); _Complex int ci = a; PI("toint", ci); _Complex double back = ci; PD("fromint", back);
	c = a; c += b; PD("+=", c); c *= 2i; PD("*=", c); c /= b; PD("/=", c); c -= 1; PD("-=", c); c++; PD("++", c);
	int k = 1; c = k ? a : 5; PD("?:", c);
	/* special values through the runtime */
	c = (inf + 1i) * (one + one * 1i); PD("inf*", c); c = (nan + nan * 1i) * (inf + 0i); PD("nan*inf", c);
	c = (one + one * 1i) / (z + z * 1i); PD("/0", c); c = (inf + 2i) / (one + one * 1i); PD("inf/", c); c = (one + 2i) / (inf + inf * 1i); PD("/inf", c);
	c = 1e300 * (1e300 + 1e300i); PD("big*", c); c = (1e-300 + 1e-300i) / (1e300 + 1e-300i); PD("tiny/", c);
	_Complex float fa = 1.25f - 3i, fb2 = 0.5f + 0.125fi, fc; fc = fa * fb2; PF("f*", fc); fc = fa / fb2; PF("f/", fc); fc = fa / (0.0f + 0.0fi); PF("f/0", fc);
	/* integer complex */
	_Complex int ia = 7 - 3i, ib = 2 + 5i, ic;
	ic = ia * ib; PI("i*", ic); ic = ia / ib; PI("i/", ic); ic = ia + ib; PI("i+", ic); ic = ~ia; PI("i~", ic); ic = ia / 2; PI("i/2", ic);
	_Complex unsigned ua = 3, ub = 7; ua /= ub; PI("u/", ua); _Complex char cc = 100 + 100i; cc = cc + cc; PI("c+", cc); PI("cneg", cneg(cc));
	PI("lmul", lmul(0x100000000LL + 3i, 5 - 0x200000000LL * 1i));
	/* the ABI */
	PD("add", add(a, b)); PF("fmix", fmix(2.0f, 1 + 1i, 3, 0.5f - 1i));
	struct S s2 = pass(gs); PF("pass.f", s2.f); PI("pass.s", s2.s); PD("va", va(3, a, b, 1.0 + 1.0i));
	printf("sizes %d %d %d %d %d\n", (int)sizeof(_Complex char), (int)sizeof(_Complex float), (int)sizeof(_Complex double), (int)__alignof__(_Complex double), (int)__alignof__(_Complex short));
	return 0;
}
