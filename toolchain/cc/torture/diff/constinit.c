/* constinit.c — static initializers GCC folds: address differences inside one object (in short / long long slots),
 * offsetof by the null-pointer idiom and from an integer address, reads of const scalar
 * globals, label differences in shorts. Must match GCC (compared through their values). */
int printf(const char *, ...);
struct { char a, b, f[3]; } s;
struct V { int c; char d; } v;
struct T { int x; struct V t; };
static short d1 = (char *)&v.d - (char *)&v;
static long long d2 = s.f - &s.b;
static unsigned long off = (unsigned long)((unsigned char *)&((struct T *)0)->t.d - (unsigned char *)0);
static struct V *const at = &((struct T *const)0x4000)->t;
const char ca = 0x42; const short cs = -7; const double cd = 2.5;
static double fa = (double)ca + 1, fb = cs * 2.0, fc = cd * 4;
static int ia = ca + cs;
static int pick(int k) {
	static const short ar[] = { &&l1 - &&l1, &&l2 - &&l1, &&l3 - &&l1 };
	goto *(&&l1 + ar[k]);
l1: return 1;
l2: return 2;
l3: return 3;
}
int main(void) {
	printf("%d %lld %lu %x %d %d %d | %d %d %d\n", d1, d2, off, (unsigned)at, (int)fa, (int)fb, (int)fc, ia, pick(1), pick(2));
	return 0;
}
