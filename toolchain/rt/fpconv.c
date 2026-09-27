/*
 * fpconv.c — 64-bit integer <-> floating conversions (RTABI 4.1.2 __aeabi_* + the libgcc names), and
 * __builtin_powi's helpers. VFP has no such instructions. The conversions use the BASE PCS (values in r0/r1,
 * even under hard float) under BOTH names — pcs("aapcs") — as GCC calls them (libgcc's __floatdidf is an alias
 * of __aeabi_l2d); only __powi[sd]f2 are ordinary (hard float: s0/d0) functions.
 *
 * Built only from 32-bit conversions and exact arithmetic, so they never call themselves:
 *  - int64 -> double: hi * 2^32 + lo — both terms are exact, so the one rounding of the sum is correct.
 *  - int64 -> float: first narrow the integer to <= 53 significant bits keeping a sticky bit (so the
 *    double is exact), then round ONCE to float — no double rounding.
 *  - double -> int64: truncate; for |d| >= 2^32, hi = trunc(d / 2^32) and lo = d - hi * 2^32 are exact.
 */

#define TWO32 4294967296.0
#define BASE __attribute__((pcs("aapcs")))

static double ul2d(unsigned long long u) { return (double)(unsigned)(u >> 32) * TWO32 + (double)(unsigned)u; }
static double l2d(long long v) { return v < 0 ? -ul2d(-(unsigned long long)v) : ul2d((unsigned long long)v); }
static float ul2f(unsigned long long u)
{
	int e = 0;
	while (u >> 53) { u = (u >> 1) | (u & 1); e++; }   /* keep the shifted-out bits as a sticky lsb */
	double d = ul2d(u);                                   /* exact: <= 53 bits */
	while (e--) d = d * 2.0;                              /* exact scaling */
	return (float)d;
}
static float l2f(long long v) { return v < 0 ? -ul2f(-(unsigned long long)v) : ul2f((unsigned long long)v); }
static unsigned long long d2ulz(double d)
{
	if (!(d >= 1.0)) return 0;                            /* < 1 (and NaN) truncates to 0 */
	if (d < TWO32) return (unsigned)d;
	unsigned hi = (unsigned)(d / TWO32);
	unsigned lo = (unsigned)(d - (double)hi * TWO32);
	return ((unsigned long long)hi << 32) | lo;
}
static long long d2lz(double d) { return d < 0 ? -(long long)d2ulz(-d) : (long long)d2ulz(d); }

BASE double __aeabi_ul2d(unsigned long long u) { return ul2d(u); }
BASE double __aeabi_l2d(long long v) { return l2d(v); }
BASE float __aeabi_ul2f(unsigned long long u) { return ul2f(u); }
BASE float __aeabi_l2f(long long v) { return l2f(v); }
BASE unsigned long long __aeabi_d2ulz(double d) { return d2ulz(d); }
BASE long long __aeabi_d2lz(double d) { return d2lz(d); }
BASE unsigned long long __aeabi_f2ulz(float f) { return d2ulz(f); }
BASE long long __aeabi_f2lz(float f) { return d2lz(f); }

BASE double __floatundidf(unsigned long long u) { return ul2d(u); }
BASE double __floatdidf(long long v) { return l2d(v); }
BASE float __floatundisf(unsigned long long u) { return ul2f(u); }
BASE float __floatdisf(long long v) { return l2f(v); }
BASE unsigned long long __fixunsdfdi(double d) { return d2ulz(d); }
BASE long long __fixdfdi(double d) { return d2lz(d); }
BASE unsigned long long __fixunssfdi(float f) { return d2ulz(f); }
BASE long long __fixsfdi(float f) { return d2lz(f); }

/* x^m by repeated squaring — libgcc's algorithm, so results match bit for bit */
double __powidf2(double x, int m)
{
	unsigned n = m < 0 ? -(unsigned)m : (unsigned)m;
	double y = n % 2 ? x : 1;
	while (n >>= 1) { x = x * x; if (n % 2) y = y * x; }
	return m < 0 ? 1 / y : y;
}
float __powisf2(float x, int m)
{
	unsigned n = m < 0 ? -(unsigned)m : (unsigned)m;
	float y = n % 2 ? x : 1;
	while (n >>= 1) { x = x * x; if (n % 2) y = y * x; }
	return m < 0 ? 1 / y : y;
}
