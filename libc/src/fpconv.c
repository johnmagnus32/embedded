/*
 * fpconv.c — the ARM run-time ABI (RTABI) helpers for 64-bit integer <-> floating conversions. VFP has no
 * such instructions, so compilers call these (libgcc provides them for GCC); we provide our own so
 * TOOLCHAIN=custom needs no libgcc. They use the base PCS (values in r0/r1), as the RTABI requires.
 *
 * Built only from 32-bit conversions and exact arithmetic, so they never call themselves:
 *  - int64 -> double: hi * 2^32 + lo — both terms are exact, so the one rounding of the sum is correct.
 *  - int64 -> float: first narrow the integer to <= 53 significant bits keeping a sticky bit (so the
 *    double is exact), then round ONCE to float — no double rounding.
 *  - double -> int64: truncate; for |d| >= 2^32, hi = trunc(d / 2^32) and lo = d - hi * 2^32 are exact.
 */

#define TWO32 4294967296.0

double __aeabi_ul2d(unsigned long long u)
{
	return (double)(unsigned)(u >> 32) * TWO32 + (double)(unsigned)u;
}

double __aeabi_l2d(long long v)
{
	return v < 0 ? -__aeabi_ul2d(-(unsigned long long)v) : __aeabi_ul2d((unsigned long long)v);
}

float __aeabi_ul2f(unsigned long long u)
{
	int e = 0;
	while (u >> 53) { u = (u >> 1) | (u & 1); e++; }   /* keep the shifted-out bits as a sticky lsb */
	double d = __aeabi_ul2d(u);                           /* exact: <= 53 bits */
	while (e--) d = d * 2.0;                              /* exact scaling */
	return (float)d;
}

float __aeabi_l2f(long long v)
{
	return v < 0 ? -__aeabi_ul2f(-(unsigned long long)v) : __aeabi_ul2f((unsigned long long)v);
}

unsigned long long __aeabi_d2ulz(double d)
{
	if (!(d >= 1.0)) return 0;                            /* < 1 (and NaN) truncates to 0 */
	if (d < TWO32) return (unsigned)d;
	unsigned hi = (unsigned)(d / TWO32);
	unsigned lo = (unsigned)(d - (double)hi * TWO32);
	return ((unsigned long long)hi << 32) | lo;
}

long long __aeabi_d2lz(double d)
{
	return d < 0 ? -(long long)__aeabi_d2ulz(-d) : (long long)__aeabi_d2ulz(d);
}

unsigned long long __aeabi_f2ulz(float f) { return __aeabi_d2ulz(f); }
long long __aeabi_f2lz(float f) { return __aeabi_d2lz(f); }
