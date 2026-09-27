/*
 * rttest.c — the runtime library's differential test. Every helper is exercised on edge cases and a
 * pseudo-random stream (explicit calls, plus the operators GCC lowers to them at -march=armv7-a: no
 * hardware divide there). The printed log must be IDENTICAL whether the program is built by GCC and
 * linked with libgcc, built by GCC and linked with our libosrt, or built by our cc with libosrt.
 */
typedef unsigned long long u64; typedef long long i64; typedef unsigned u32;

static void putc_(char c) { *(volatile unsigned char *)0x09000000 = (unsigned char)c; }
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void hex(u64 v, int digits) { for (int i = digits - 1; i >= 0; i--) putc_("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void out(const char *tag, u64 v) { puts_(tag); putc_(' '); hex(v, 16); putc_('\n'); }

static volatile int fpe;   /* volatile: GCC assumes division libcalls don't touch memory */
int raise(int sig) { fpe = sig; return 0; }   /* the div0 hooks call raise(SIGFPE) */
void abort(void) { puts_("abort\n"); for (;;) ; }   /* -ftrapv overflow (not exercised) */

#define BASE __attribute__((pcs("aapcs")))
BASE double __aeabi_l2d(i64); BASE double __aeabi_ul2d(u64); BASE float __aeabi_l2f(i64); BASE float __aeabi_ul2f(u64);
BASE i64 __aeabi_d2lz(double); BASE u64 __aeabi_d2ulz(double); BASE i64 __aeabi_f2lz(float); BASE u64 __aeabi_f2ulz(float);
BASE double __floatdidf(i64); BASE double __floatundidf(u64); BASE float __floatdisf(i64); BASE float __floatundisf(u64);
BASE i64 __fixdfdi(double); BASE u64 __fixunsdfdi(double); BASE i64 __fixsfdi(float); BASE u64 __fixunssfdi(float);
BASE unsigned short __gnu_f2h_ieee(float); BASE float __gnu_h2f_ieee(unsigned short); BASE unsigned short __gnu_d2h_ieee(double);   /* GCC: base-PCS libcalls */
double __powidf2(double, int); float __powisf2(float, int);
int __aeabi_idiv(int, int); unsigned __aeabi_uidiv(unsigned, unsigned);
int __divsi3(int, int); int __modsi3(int, int); unsigned __udivsi3(unsigned, unsigned); unsigned __umodsi3(unsigned, unsigned);
i64 __divdi3(i64, i64); i64 __moddi3(i64, i64); u64 __udivdi3(u64, u64); u64 __umoddi3(u64, u64);
u64 __udivmoddi4(u64, u64, u64 *);
i64 __aeabi_llsl(i64, int); i64 __aeabi_llsr(i64, int); i64 __aeabi_lasr(i64, int);
i64 __ashldi3(i64, int); i64 __lshrdi3(i64, int); i64 __ashrdi3(i64, int);
int __aeabi_lcmp(i64, i64); int __aeabi_ulcmp(u64, u64); int __cmpdi2(i64, i64); int __ucmpdi2(u64, u64);
i64 __aeabi_lmul(i64, i64); i64 __muldi3(i64, i64); i64 __negdi2(i64);
int __clzsi2(u32); int __clzdi2(u64); int __ctzsi2(u32); int __ctzdi2(u64); int __ffsdi2(u64);
int __popcountsi2(u32); int __popcountdi2(u64); int __paritysi2(u32); int __paritydi2(u64);
u32 __bswapsi2(u32); u64 __bswapdi2(u64); int __clrsbsi2(int); int __clrsbdi2(i64);
int __addvsi3(int, int); int __subvsi3(int, int); int __mulvsi3(int, int); i64 __addvdi3(i64, i64); i64 __mulvdi3(i64, i64);

static u64 st = 0x9e3779b97f4a7c15ULL;
static u64 rnd(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }
static u64 rndmag(void) { u64 v = rnd(); int sh = (int)(rnd() & 63); return v >> sh; }   /* every magnitude */
static u64 dbits(double d) { union { double d; u64 u; } v; v.d = d; return v.u; }
static u32 fbits(float f) { union { float f; u32 u; } v; v.f = f; return v.u; }
static double bitsd(u64 u) { union { double d; u64 u; } v; v.u = u; return v.d; }
static float bitsf(u32 u) { union { float f; u32 u; } v; v.u = u; return v.f; }
static u64 h = 1469598103934665603ULL;
static const char *grp = "";
#ifdef VERBOSE   /* -DVERBOSE='"fpconv"': print every value of that group (to diff the three builds) */
static int vn;
static int streq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static void mix(u64 v) { if (streq(grp, VERBOSE)) { hex((u64)vn++, 5); putc_(' '); hex(v, 16); putc_('\n'); } for (int i = 0; i < 8; i++) { h ^= (v >> (8 * i)) & 0xff; h *= 1099511628211ULL; } }
#else
static void mix(u64 v) { for (int i = 0; i < 8; i++) { h ^= (v >> (8 * i)) & 0xff; h *= 1099511628211ULL; } }
#endif

int main(void)
{
	static const i64 edge[] = { 0, 1, -1, 2, -2, 7, -7, 0x7fffffff, -0x7fffffff - 1, 0x80000000LL, 0xffffffffLL, 0x100000000LL,
		0x7fffffffffffffffLL, -0x7fffffffffffffffLL - 1, 12345678901234LL, -98765432109876LL };
	int ne = sizeof edge / sizeof *edge;
	volatile i64 a, b; volatile u64 ua, ub; volatile int ia, ib; volatile u32 uia, uib;

	grp = "div-edge";
	/* division: operators (lowered to __aeabi_{u,}{i,l}divmod by GCC at armv7-a; ours calls the l forms) + names */
	for (int i = 0; i < ne; i++) for (int j = 0; j < ne; j++) {
		a = edge[i]; b = edge[j]; if (!b || (a == -0x7fffffffffffffffLL - 1 && b == -1)) continue;
		mix((u64)(a / b)); mix((u64)(a % b)); ua = (u64)a; ub = (u64)b; mix(ua / ub); mix(ua % ub);
		mix((u64)__divdi3(a, b)); mix((u64)__moddi3(a, b)); mix(__udivdi3(ua, ub)); mix(__umoddi3(ua, ub));
		ia = (int)a; ib = (int)b; if (ib && !(ia == -0x7fffffff - 1 && ib == -1)) { mix((u64)(ia / ib)); mix((u64)(ia % ib)); mix((u64)__divsi3(ia, ib)); mix((u64)__modsi3(ia, ib)); mix((u64)__aeabi_idiv(ia, ib)); }
		uia = (u32)a; uib = (u32)b; if (uib) { mix(uia / uib); mix(uia % uib); mix(__udivsi3(uia, uib)); mix(__umodsi3(uia, uib)); mix(__aeabi_uidiv(uia, uib)); }
	}
	out("div-edge", h);
	h = 1469598103934665603ULL; grp = "div-rand";
	for (int k = 0; k < 3000; k++) {
		ua = rndmag(); ub = rndmag(); if (!ub) ub = 1; a = (i64)ua; b = (i64)ub;
		if (!(a == -0x7fffffffffffffffLL - 1 && b == -1)) { mix((u64)(a / b)); mix((u64)(a % b)); }
		mix(ua / ub); mix(ua % ub); { u64 r; mix(__udivmoddi4(ua, ub, &r)); mix(r); }
		uia = (u32)ua; uib = (u32)ub | 1; mix(uia / uib); mix(uia % uib);
		ia = (int)ua; ib = (int)ub | 1; if (!(ia == -0x7fffffff - 1 && ib == -1)) { mix((u64)(ia / ib)); mix((u64)(ia % ib)); }
	}
	out("div-rand", h);
	/* division by zero: libgcc's results + SIGFPE */
	fpe = 0; out("uidiv0", __aeabi_uidiv(5, 0)); out("fpe", (u64)fpe);
	fpe = 0; out("idiv0+", (u64)(u32)__aeabi_idiv(5, 0)); out("idiv0-", (u64)(u32)__aeabi_idiv(-5, 0)); out("fpe", (u64)fpe);
	fpe = 0; ua = 9; ub = 0; out("uldiv0", ua / ub); a = -9; b = 0; out("ldiv0-", (u64)(a / b)); a = 9; out("ldiv0+", (u64)(a / b)); out("fpe", (u64)fpe);

	/* shifts / compares / multiply */
	h = 1469598103934665603ULL; grp = "int64";
	for (int k = 0; k < 2000; k++) {
		i64 x = (i64)rnd(), y = (i64)rndmag(); int n = (int)(rnd() & 63);
		mix((u64)__aeabi_llsl(x, n)); mix((u64)__aeabi_llsr(x, n)); mix((u64)__aeabi_lasr(x, n));
		mix((u64)__ashldi3(x, n)); mix((u64)__lshrdi3(x, n)); mix((u64)__ashrdi3(x, n));
		mix((u64)__aeabi_lcmp(x, y)); mix((u64)__aeabi_ulcmp((u64)x, (u64)y)); mix((u64)__cmpdi2(x, y)); mix((u64)__ucmpdi2((u64)x, (u64)y));
		mix((u64)__aeabi_lcmp(x, x)); mix((u64)__cmpdi2(y, y));
		mix((u64)__aeabi_lmul(x, y)); mix((u64)__muldi3(x, y)); mix((u64)__negdi2(x));
	}
	out("int64", h);

	/* bits */
	h = 1469598103934665603ULL; grp = "bits";
	for (int k = 0; k < 2000; k++) {
		u64 v = k < 64 ? 1ULL << k : k < 70 ? (u64)(k - 64) : rndmag(); u32 w = (u32)v;
		if (w) { mix((u64)__clzsi2(w)); mix((u64)__ctzsi2(w)); }
		if (v) { mix((u64)__clzdi2(v)); mix((u64)__ctzdi2(v)); }
		mix((u64)__ffsdi2(v)); mix((u64)__popcountsi2(w)); mix((u64)__popcountdi2(v)); mix((u64)__paritysi2(w)); mix((u64)__paritydi2(v));
		mix(__bswapsi2(w)); mix(__bswapdi2(v)); mix((u64)__clrsbsi2((int)w)); mix((u64)__clrsbdi2((i64)v)); mix((u64)__clrsbsi2(-(int)w));
	}
	out("bits", h);

	/* -ftrapv (non-overflowing operands: an overflow traps) */
	h = 1469598103934665603ULL; grp = "trapv";
	for (int k = 0; k < 500; k++) {
		int x = (int)(rnd() >> 49) - 16384, y = (int)(rnd() >> 49) - 16384; i64 X = (i64)(rnd() >> 34) - 0x20000000, Y = (i64)(rnd() >> 34);
		mix((u64)__addvsi3(x, y)); mix((u64)__subvsi3(x, y)); mix((u64)__mulvsi3(x, y)); mix((u64)__addvdi3(X, Y)); mix((u64)__mulvdi3(X, Y));
	}
	out("trapv", h);

	/* int64 <-> FP: RTABI (base PCS) and libgcc names */
	h = 1469598103934665603ULL; grp = "fpconv";
	for (int k = 0; k < 3000; k++) {
		i64 v = k < ne ? edge[k] : (i64)rndmag() * ((rnd() & 1) ? -1 : 1); u64 u = (u64)v;
		mix(dbits(__aeabi_l2d(v))); mix(dbits(__aeabi_ul2d(u))); mix(fbits(__aeabi_l2f(v))); mix(fbits(__aeabi_ul2f(u)));
		mix(dbits(__floatdidf(v))); mix(dbits(__floatundidf(u))); mix(fbits(__floatdisf(v))); mix(fbits(__floatundisf(u)));
		mix(dbits((double)v)); mix(dbits((double)u)); mix(fbits((float)v)); mix(fbits((float)u));
		double d = bitsd((rnd() & 0x800fffffffffffffULL) | ((u64)(1023 + (int)(rnd() % 62)) << 52));   /* |d| in [1, 2^62) */
		float f = bitsf((u32)((rnd() & 0x807fffffu) | ((u64)(127 + (int)(rnd() % 62)) << 23)));
		mix((u64)__aeabi_d2lz(d)); mix((u64)__fixdfdi(d)); mix((u64)__aeabi_f2lz(f)); mix((u64)__fixsfdi(f)); mix((u64)(i64)d); mix((u64)(i64)f);
		double ad = d < 0 ? -d : d; float af = f < 0 ? -f : f;
		mix(__aeabi_d2ulz(ad)); mix(__fixunsdfdi(ad)); mix(__aeabi_f2ulz(af)); mix(__fixunssfdi(af)); mix((u64)ad); mix((u64)af);
		mix(__aeabi_d2lz(0.75)); mix(__aeabi_d2ulz(-0.5));
	}
	out("fpconv", h);
	h = 1469598103934665603ULL; grp = "powi";
	for (int k = -40; k <= 40; k++) { mix(dbits(__powidf2(1.0000001, k * 1000))); mix(dbits(__powidf2(-3.5, k))); mix(fbits(__powisf2(1.5f, k))); mix(fbits(__powisf2(-0.3f, k))); }
	out("powi", h);

	/* half precision: every half -> float, and floats across the whole range -> half */
	h = 1469598103934665603ULL; grp = "h2f";
	for (u32 x = 0; x < 65536; x++) mix(fbits(__gnu_h2f_ieee((unsigned short)x)));
	out("h2f", h);
	h = 1469598103934665603ULL; grp = "f2h";
	for (int k = 0; k < 20000; k++) { float f = bitsf((u32)rnd()); if (k < 256) f = bitsf((u32)k << 23 | (u32)(rnd() & 0x7fffff)); mix(__gnu_f2h_ieee(f)); }
	out("f2h", h);
	h = 1469598103934665603ULL; grp = "d2h";
	for (int k = 0; k < 20000; k++) {   /* doubles around the half range, incl. exact ties + one-ulp-off ties */
		double d = bitsd((rnd() & 0x800fffffffffffffULL) | ((u64)(1023 - 30 + (int)(rnd() % 50)) << 52));
		if (k < 2048) d = bitsd(((u64)(k & 1) << 63) | ((u64)(1023 - 26 + k / 64) << 52) | ((u64)(k & 62) << 36) | (k & 4 ? 1ULL << 41 : 0) | (k & 8 ? 1 : 0));
		if (k >= 19990) d = bitsd(k & 1 ? 0x7ff0000000000000ULL : 0x7ff8000000000123ULL);
		mix(__gnu_d2h_ieee(d));
	}
	out("d2h", h);

	return 0;
}
