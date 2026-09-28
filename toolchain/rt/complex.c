/*
 * complex.c — floating complex multiplication and division (libgcc's __mulsc3/__muldc3/__divsc3/__divdc3), which
 * the compiler calls for `_Complex * _Complex` and `/` (C99 Annex G semantics). The results are bit-identical to
 * libgcc's, so the operation order below is the one it uses:
 *
 *  multiply   (a+bi)(c+di) = (ac - bd) + (ad + bc)i; when that comes out NaN+NaN from an infinite operand or an
 *             overflowing product, the infinity is recovered: an infinite factor is "boxed" to +-1 / +-0 per part,
 *             NaN parts of the other factor become 0, and the product is recomputed scaled by infinity.
 *  divide     double: Smith's method (divide through by the larger of c, d), with the range checks that keep the
 *             denominator from overflowing or the quotient from underflowing (halve everything near DBL_MAX, scale
 *             everything up by 1/eps when tiny) and the alternate order when the ratio is subnormal. float: in
 *             double precision with the textbook formula — exact enough there, as libgcc does given hardware
 *             double. Then NaN+NaN results are repaired: x/0 is an infinity, inf/finite an infinity, finite/inf
 *             a zero, each with the right signs.
 *
 * libgcc is built with floating contraction, so its sums of products are fused (one rounding): the recomputations
 * after a NaN, every division formula, and float division's double-precision one. FMA/FMAF mark exactly those
 * places — identical results need identical rounding. Complex values are built part by part (never `x + y*I`):
 * this file must not call itself.
 */
#define FMA  __builtin_fma
#define FMAF __builtin_fmaf
#define ISNAN(x) __builtin_isnan(x)
#define ISINF(x) __builtin_isinf(x)
#define FIN(x)   __builtin_isfinite(x)

_Complex float __mulsc3(float a, float b, float c, float d)
{
	float ac = a * c, bd = b * d, ad = a * d, bc = b * c, x = ac - bd, y = ad + bc;
	if (ISNAN(x) && ISNAN(y)) {
		int again = 0;
		if (ISINF(a) || ISINF(b)) {   /* the first factor is infinite */
			a = __builtin_copysignf(ISINF(a) ? 1 : 0, a); b = __builtin_copysignf(ISINF(b) ? 1 : 0, b);
			if (ISNAN(c)) c = __builtin_copysignf(0, c);
			if (ISNAN(d)) d = __builtin_copysignf(0, d);
			again = 1;
		}
		if (ISINF(c) || ISINF(d)) {   /* the second */
			c = __builtin_copysignf(ISINF(c) ? 1 : 0, c); d = __builtin_copysignf(ISINF(d) ? 1 : 0, d);
			if (ISNAN(a)) a = __builtin_copysignf(0, a);
			if (ISNAN(b)) b = __builtin_copysignf(0, b);
			again = 1;
		}
		if (!again && (ISINF(ac) || ISINF(bd) || ISINF(ad) || ISINF(bc))) {   /* a product overflowed */
			if (ISNAN(a)) a = __builtin_copysignf(0, a);
			if (ISNAN(b)) b = __builtin_copysignf(0, b);
			if (ISNAN(c)) c = __builtin_copysignf(0, c);
			if (ISNAN(d)) d = __builtin_copysignf(0, d);
			again = 1;
		}
		if (again) { x = __builtin_huge_valf() * FMAF(a, c, -(b * d)); y = __builtin_huge_valf() * FMAF(a, d, b * c); }
	}
	_Complex float r; __real__ r = x; __imag__ r = y; return r;
}

_Complex double __muldc3(double a, double b, double c, double d)
{
	double ac = a * c, bd = b * d, ad = a * d, bc = b * c, x = ac - bd, y = ad + bc;
	if (ISNAN(x) && ISNAN(y)) {
		int again = 0;
		if (ISINF(a) || ISINF(b)) {
			a = __builtin_copysign(ISINF(a) ? 1 : 0, a); b = __builtin_copysign(ISINF(b) ? 1 : 0, b);
			if (ISNAN(c)) c = __builtin_copysign(0, c);
			if (ISNAN(d)) d = __builtin_copysign(0, d);
			again = 1;
		}
		if (ISINF(c) || ISINF(d)) {
			c = __builtin_copysign(ISINF(c) ? 1 : 0, c); d = __builtin_copysign(ISINF(d) ? 1 : 0, d);
			if (ISNAN(a)) a = __builtin_copysign(0, a);
			if (ISNAN(b)) b = __builtin_copysign(0, b);
			again = 1;
		}
		if (!again && (ISINF(ac) || ISINF(bd) || ISINF(ad) || ISINF(bc))) {
			if (ISNAN(a)) a = __builtin_copysign(0, a);
			if (ISNAN(b)) b = __builtin_copysign(0, b);
			if (ISNAN(c)) c = __builtin_copysign(0, c);
			if (ISNAN(d)) d = __builtin_copysign(0, d);
			again = 1;
		}
		if (again) { x = __builtin_huge_val() * FMA(a, c, -(b * d)); y = __builtin_huge_val() * FMA(a, d, b * c); }
	}
	_Complex double r; __real__ r = x; __imag__ r = y; return r;
}

_Complex float __divsc3(float a, float b, float c, float d)
{
	double A = a, B = b, C = c, D = d, den = FMA(C, C, D * D);
	float x = FMA(A, C, B * D) / den, y = FMA(B, C, -(A * D)) / den;
	if (ISNAN(x) && ISNAN(y)) {   /* recover the infinities and zeros */
		if (c == 0.0 && d == 0.0 && (!ISNAN(a) || !ISNAN(b))) {
			x = __builtin_copysignf(__builtin_huge_valf(), c) * a; y = __builtin_copysignf(__builtin_huge_valf(), c) * b;
		} else if ((ISINF(a) || ISINF(b)) && FIN(c) && FIN(d)) {
			a = __builtin_copysignf(ISINF(a) ? 1 : 0, a); b = __builtin_copysignf(ISINF(b) ? 1 : 0, b);
			x = __builtin_huge_valf() * FMAF(a, c, b * d); y = __builtin_huge_valf() * FMAF(b, c, -(a * d));
		} else if ((ISINF(c) || ISINF(d)) && FIN(a) && FIN(b)) {
			c = __builtin_copysignf(ISINF(c) ? 1 : 0, c); d = __builtin_copysignf(ISINF(d) ? 1 : 0, d);
			x = 0.0 * FMAF(a, c, b * d); y = 0.0 * FMAF(b, c, -(a * d));
		}
	}
	_Complex float r; __real__ r = x; __imag__ r = y; return r;
}

#define DBIG     (1.7976931348623157e308 / 2)     /* DBL_MAX / 2: halve the operands at or above it */
#define DMIN     2.2250738585072014e-308          /* DBL_MIN */
#define DEPS     2.220446049250313e-16            /* DBL_EPSILON: below it, scale everything up by 1/eps */
#define DSCALE   (1 / DEPS)
#define DMAX2    (DBIG * DEPS)
_Complex double __divdc3(double a, double b, double c, double d)
{
	double ratio, den, x, y;
	int dbig = __builtin_fabs(c) < __builtin_fabs(d);   /* divide through by the larger of c and d */
	double big = dbig ? d : c, small = dbig ? c : d;
	if (__builtin_fabs(big) >= DBIG) { a = a / 2; b = b / 2; c = c / 2; d = d / 2; big = big / 2; small = small / 2; }
	if (__builtin_fabs(big) < DEPS) { a = a * DSCALE; b = b * DSCALE; c = c * DSCALE; d = d * DSCALE; big = big * DSCALE; small = small * DSCALE; }
	else if ((__builtin_fabs(a) < DMIN && __builtin_fabs(b) < DMAX2 && __builtin_fabs(big) < DMAX2)
	         || (__builtin_fabs(b) < DMIN && __builtin_fabs(a) < DMAX2 && __builtin_fabs(big) < DMAX2)) {
		a = a * DSCALE; b = b * DSCALE; c = c * DSCALE; d = d * DSCALE; big = big * DSCALE; small = small * DSCALE;
	}
	ratio = small / big; den = FMA(small, ratio, big);
	if (dbig) {   /* (a + bi) / (c + di), d dominant */
		if (__builtin_fabs(ratio) > DMIN) { x = FMA(a, ratio, b) / den; y = FMA(b, ratio, -a) / den; }
		else { x = FMA(a / d, c, b) / den; y = FMA(b / d, c, -a) / den; }   /* a subnormal ratio: the other order */
	} else {
		if (__builtin_fabs(ratio) > DMIN) { x = FMA(b, ratio, a) / den; y = FMA(-a, ratio, b) / den; }
		else { x = FMA(b / c, d, a) / den; y = FMA(-(a / c), d, b) / den; }
	}
	if (ISNAN(x) && ISNAN(y)) {
		if (c == 0.0 && d == 0.0 && (!ISNAN(a) || !ISNAN(b))) {
			x = __builtin_copysign(__builtin_huge_val(), c) * a; y = __builtin_copysign(__builtin_huge_val(), c) * b;
		} else if ((ISINF(a) || ISINF(b)) && FIN(c) && FIN(d)) {
			a = __builtin_copysign(ISINF(a) ? 1 : 0, a); b = __builtin_copysign(ISINF(b) ? 1 : 0, b);
			x = __builtin_huge_val() * FMA(a, c, b * d); y = __builtin_huge_val() * FMA(b, c, -(a * d));
		} else if ((ISINF(c) || ISINF(d)) && FIN(a) && FIN(b)) {
			c = __builtin_copysign(ISINF(c) ? 1 : 0, c); d = __builtin_copysign(ISINF(d) ? 1 : 0, d);
			x = 0.0 * FMA(a, c, b * d); y = 0.0 * FMA(b, c, -(a * d));
		}
	}
	_Complex double r; __real__ r = x; __imag__ r = y; return r;
}
