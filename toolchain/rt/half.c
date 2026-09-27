/*
 * half.c — IEEE binary16 <-> binary32 (libgcc's fp16.c algorithm: round to nearest even, NaNs kept quiet
 * with their top payload bits, subnormals both ways) — libgcc's __gnu_*_ieee names, called for __fp16 with
 * -mfp16-format=ieee when the FPU has no half conversions. Base PCS (pcs("aapcs")), as GCC calls them.
 */
#define BASE __attribute__((pcs("aapcs")))

static unsigned f2h(unsigned a)
{
	unsigned sign = (a >> 16) & 0x8000, mantissa = a & 0x007fffff, mask, increment;
	int aexp = (a >> 23) & 0xff;
	if (aexp == 0xff) return mantissa ? sign | 0x7e00 | (mantissa >> 13) : sign | 0x7c00;   /* NaN (quiet) / inf */
	if (aexp == 0 && mantissa == 0) return sign;
	aexp -= 127;
	mantissa |= 0x00800000;                               /* the implicit 1, at bit 23 */
	if (aexp < -14) { mask = 0x00ffffff; if (aexp >= -25) mask >>= 25 + aexp; }   /* becomes subnormal */
	else mask = 0x00001fff;
	if (mantissa & mask) {                                /* round to nearest, ties to even */
		increment = (mask + 1) >> 1;
		if ((mantissa & mask) == increment) increment = mantissa & (increment << 1);
		mantissa += increment;
		if (mantissa >= 0x01000000) { mantissa >>= 1; aexp++; }
	}
	if (aexp > 15) return sign | 0x7c00;                  /* overflow -> inf */
	if (aexp < -24) return sign;                          /* underflow -> 0 */
	if (aexp < -14) { mantissa >>= -14 - aexp; aexp = -14; }
	return sign | (((unsigned)(aexp + 14) << 10) + (mantissa >> 13));   /* the kept leading 1 adds the bias's missing 1 */
}
static unsigned h2f(unsigned a)
{
	unsigned sign = (a & 0x8000) << 16, mantissa = a & 0x3ff;
	int aexp = (a >> 10) & 0x1f;
	if (aexp == 0x1f) return sign | 0x7f800000 | (mantissa << 13);   /* inf / NaN */
	if (aexp == 0) {
		if (!mantissa) return sign;
		int shift = __builtin_clz(mantissa) - 21;         /* normalize a subnormal */
		mantissa <<= shift; aexp = -shift;
	}
	return sign | (((unsigned)(aexp + 0x70) << 23) + (mantissa << 13));
}

static unsigned fbits(float f) { union { float f; unsigned u; } v; v.f = f; return v.u; }
static float bitsf(unsigned u) { union { float f; unsigned u; } v; v.u = u; return v.f; }

BASE unsigned short __gnu_f2h_ieee(float f) { return (unsigned short)f2h(fbits(f)); }
BASE float __gnu_h2f_ieee(unsigned short h) { return bitsf(h2f(h)); }
/* double -> half, rounding ONCE: narrow to float keeping a sticky bit for everything a float can't hold (so a
 * tie in half precision stays distinguishable), then f2h. */
BASE unsigned short __gnu_d2h_ieee(double d)
{
	union { double d; unsigned long long u; } v; v.d = d;
	unsigned long long u = v.u; unsigned sign = (unsigned)(u >> 48) & 0x8000; int e = (int)((u >> 52) & 0x7ff);
	unsigned long long m = u & 0xfffffffffffffULL;
	if (e == 0x7ff) return (unsigned short)(m ? sign | 0x7e00 | (unsigned)(m >> 42) : sign | 0x7c00);
	if (e == 0 && m == 0) return (unsigned short)sign;
	e -= 1023;
	if (e > 15) return (unsigned short)(sign | 0x7c00);
	if (e < -26) return (unsigned short)sign;             /* below half of the smallest subnormal: +-0 */
	unsigned fm = (unsigned)(m >> 29) | ((m & ((1ULL << 29) - 1)) != 0);   /* 23 bits + sticky */
	return (unsigned short)f2h((sign << 16) | ((unsigned)(e + 127) << 23) | fm);
}
