// expect: 255
/* Evaluation + conversion rules GCC-built code relies on:
 * op= / ++ evaluate the lvalue ONCE and the operand BEFORE reading it (GCC order: x |= f() sees f's store);
 * an assignment's value is converted to the lhs type; a narrow return is narrowed by the callee (AAPCS);
 * _Bool conversion is `!= 0`; an enum with no negative values is unsigned (so an enum:2 field holds 3);
 * a 64-bit condition tests BOTH words; a 64-bit switch compares the pair (and unsigned ranges). */
typedef _Bool bool;
enum pos { P0, P1, P2, P3 };
struct bf { enum pos p : 2; bool b : 1; };
static int calls, x0;
static int *once(int *p) { calls++; return p; }
static int bump(void) { x0 |= 128; return 1; }
static unsigned char nar(unsigned x) { return x << 4; }                /* 0x123 << 4 -> 0x30 */
static bool tob(int v) { return v; }
static int sw(unsigned long long v) { switch (v) { case 0x100000000ULL: return 1; case 5 ... 9: return 2; default: return 3; } }
int main(void) {
	int r = 0, a[2] = { 1, 1 }, i = 0;
	a[i++] += 5; *once(&a[1]) += 1; r += (a[0] == 6 && a[1] == 2 && i == 1 && calls == 1);          /* 1 */
	x0 = 2; x0 |= bump(); r += (x0 == 131) << 1;                                                    /* 2 */
	unsigned char uc = 255; int post = uc++; unsigned short us; int v = (us = -1);
	r += (post == 255 && uc == 0 && v == 65535) << 2;                                               /* 4 */
	r += (nar(0x123) == 0x30 && tob(256) == 1) << 3;                                                /* 8 */
	bool b = 0x100; long long big = 1LL << 40; bool c = big; r += (b == 1 && c == 1) << 4;           /* 16 */
	struct bf s; s.p = P3; s.b = 4; r += (s.p == 3 && s.b == 1) << 5;                               /* 32 */
	int t = 0; if (big) t = 1; r += (t && (big ? 1 : 0) && !!big) << 6;                               /* 64 */
	r += (sw(0x100000000ULL) == 1 && sw(7) == 2 && sw(0x7) == 2 && sw(0x500000007ULL) == 3) << 7;   /* 128 */
	return r;
}
