/* sso.c — scalar_storage_order("big-endian"): every scalar kind, normal/wide/signed bit-fields and arrays of scalars,
 * global + local, modified by =, op=, ++/--; prints the values and the raw byte images (must match GCC). */
int printf(const char *, ...);
int arrays(void);
struct __attribute__((scalar_storage_order("big-endian"))) S {
	unsigned int u; short s; unsigned char c; long long ll; double d; float f;
	int a : 3; int b : 13; unsigned c16 : 16; unsigned long long w : 40; unsigned long long w2 : 24;
};
struct S g = { 0x11223344, -2, 0x55, 0x0102030405060708LL, 1.5, -2.25f, -3, 1000, 0xBEEF, 0x123456789AULL, 0xABCDEF };
static void dump(const char *tag, struct S *p) {
	unsigned char *b = (unsigned char *)p;
	printf("%s: %x %d %x %llx %d %d %d %d %x %llx %llx |", tag, p->u, p->s, p->c, p->ll, (int)(p->d * 4), (int)(p->f * 4), p->a, p->b, p->c16, (unsigned long long)p->w, (unsigned long long)p->w2);
	for (unsigned i = 0; i < sizeof *p; i++) printf(" %02x", b[i]);
	printf("\n");
}
int main(void) {
	dump("g", &g);
	struct S l = { 0xCAFEBABE, 300, 7, -5, -0.5, 8.0f, 2, -77, 1, 0xFFFFFFFFFFULL, 5 };
	dump("l", &l);
	l.u += 1; l.s -= 301; l.c++; l.ll *= 3; l.d += 1; l.f /= 2; l.a = -4; l.b += 10; l.c16--; l.w += 2; l.w2 = l.w2 << 3;
	g.u = l.u ^ 0xff; g.a++; g.b = g.b * 2;
	dump("l2", &l); dump("g2", &g); arrays();
	return 0;
}
struct __attribute__((scalar_storage_order("big-endian"))) A { unsigned short h[3]; int w[2]; unsigned char c; };
struct A ga = { { 0x0102, 0x0304, 0x0506 }, { -2, 0x11223344 }, 9 };
int arrays(void) {
	struct A la = { { 7, 8, 9 }, { 100000, -100000 }, 3 };
	la.h[1] += 0x100; la.w[0]++; ga.w[1] = ga.w[1] + la.h[2]; int i = 2; la.h[i] = la.h[i] * 3;
	unsigned char *b = (unsigned char *)&la, *c = (unsigned char *)&ga;
	for (unsigned k = 0; k < sizeof la; k++) printf(" %02x", b[k]);
	for (unsigned k = 0; k < sizeof ga; k++) printf(" %02x", c[k]);
	printf(" | %d %d %d %d\n", la.h[1], la.w[0], ga.w[1], la.h[2]);
	return 0;
}
