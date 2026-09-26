// expect: 255
/* Hard-float: float/double literals (incl. hex float + f suffix) and constant folding in static data;
 * arithmetic, conversions (int/unsigned <-> float/double, float <-> double, to bool = != 0), ordered compares
 * false on NaN, truthiness, op= / ++ on doubles; AAPCS-VFP calls: FP args in s/d registers with back-filling,
 * FP returns, an HFA through a function pointer, a variadic double (base PCS). */
typedef __builtin_va_list va_list;
double g = 1.5, h = -0.25e1, hx = 0x1.8p1;   /* 1.5, -2.5, 3.0 */
float gf = 3.0f; int gi = 7.9; unsigned gu = 4e9;
struct v2 { float x, y; };
static double mix(int a, double b, float c, int d, float e) { return a + b * 10 + c * 100 + d * 1000 + e * 10000; }
static struct v2 add2(struct v2 p, struct v2 q) { struct v2 r = { p.x + q.x, p.y + q.y }; return r; }
static double first_double(int n, ...) { va_list ap; __builtin_va_start(ap, n); double v = __builtin_va_arg(ap, double); __builtin_va_end(ap); return v; }
int main(void) {
	int r = 0;
	double a = g * 2 + h;                                                        /* 0.5 */
	r += (a == 0.5 && hx == 3.0 && gi == 7 && gu == 4000000000u);                /* 1 */
	float f = gf / 2; r += (f > 1.4f && f < 1.6 && (int)(g * 3.0) == 4) << 1;    /* 2 */
	unsigned char uc = 200.7; double z = 0.0, nan = z / z;
	r += (uc == 200 && !(nan < 1) && !(nan >= 1) && nan != nan && !z && nan) << 2;   /* 4 */
	a += 1; a++; float q = 0.1f; double dq = q;
	r += (a == 2.5 && dq != 0.1 && (float)dq == q && (_Bool)0.25 == 1) << 3;    /* 8 */
	r += (mix(1, 2.0, 3.0f, 4, 5.0f) == 54321.0) << 4;                           /* 16: c -> s2, e back-fills s3 */
	struct v2 (*fp)(struct v2, struct v2) = add2; struct v2 p = { 1, 2 }, s = fp(p, p);
	r += (s.x == 2 && s.y == 4) << 5;                                            /* 32 */
	r += (first_double(1, 6.25) == 6.25) << 6;                                   /* 64 */
	r += (sizeof(long double) == 8 && sizeof 1.0f == 4 && -a < 0 && (unsigned)3.99 == 3) << 7;   /* 128 */
	return r;
}
