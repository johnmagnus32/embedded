/* attrs.c — attributes that change behavior or layout: cleanup (every way out of a scope: block end, return, break,
 * continue, goto, a for-init guard, a ({...}) value), packed on members and enums, a packed struct with an aligned
 * member. Prints the cleanup order and the layouts (must match GCC). */
int printf(const char *, ...);
static int depth;
static void out(int *p) { printf(" ~%d", *p); depth--; }
static void out_s(struct { int a, b; } *p) { printf(" ~s%d", p->a + p->b); }
#define G(n) int g##n __attribute__((cleanup(out))) = (depth++, n)
static int ret(int k) {
	G(1);
	{ G(2); if (k == 0) return g2 * 10 + g1; }   /* the value is computed before the cleanups run */
	G(3);
	return g3;
}
static void loops(void) {
	for (int i = 0; i < 3; i++) { G(10); if (i == 1) continue; { G(11); if (i == 2) break; } }
	printf(" |");
	int n = 0;
	while (n < 5) { G(20); n++; switch (n) { case 1: { G(21); break; } case 2: continue; default: if (n == 4) goto done; } }
done:
	printf(" | n=%d", n);
	for (int once __attribute__((cleanup(out))) = 30, *d = 0; !d; d = (int *)1) printf(" in");   /* the kernel's scoped_guard */
}
static int jumps(int k) {
	G(40);
	{ G(41); { G(42); if (k) goto out1; } G(43); }
out1:
	{ G(44); if (k > 1) goto out2; }
	{ G(45); }
out2:
	return ({ G(46); g40 + g46 + k; });
}
struct P1 { char c; int i __attribute__((packed)); short s; };
struct P2 { char c; int i : 20 __attribute__((packed)); char d; };
struct P3 { char c; long long l __attribute__((aligned(8))); } __attribute__((packed));
struct P4 { char c; int i; } __attribute__((packed, aligned(4)));
enum __attribute__((packed)) E1 { A1 = 1, B1 = 200 };
enum E2 { A2 = -1, B2 = 100 } __attribute__((packed));
enum E3 { A3 = 70000 } __attribute__((packed));
enum E4 { A4 = -200 } __attribute__((packed));
enum E5 { A5 = 40000 } __attribute__((__packed__));
struct holds { char c; enum E1 e; enum E4 f; };
int main(void) {
	printf("ret0:"); int r = ret(0); printf(" = %d depth %d\n", r, depth);
	printf("ret1:"); r = ret(1); printf(" = %d depth %d\n", r, depth);
	printf("loops:"); loops(); printf(" depth %d\n", depth);
	for (int k = 0; k < 3; k++) { printf("jumps%d:", k); r = jumps(k); printf(" = %d depth %d\n", r, depth); }
	{ struct { int a, b; } s __attribute__((cleanup(out_s))) = { 3, 4 }, t __attribute__((cleanup(out_s))) = { 5, 6 }; printf("structs:"); }
	printf("\n");
	struct P1 p1 = { 1, 2, 3 }; struct P2 p2 = { 1, 0x12345, 7 };
	printf("P1 %d %d %d %d | P2 %d %d %x %d | P3 %d %d | P4 %d %d\n", (int)sizeof p1, (int)__alignof__(p1), (int)__builtin_offsetof(struct P1, s), p1.i,
	       (int)sizeof p2, (int)__alignof__(p2), p2.i, p2.d, (int)sizeof(struct P3), (int)__alignof__(struct P3), (int)sizeof(struct P4), (int)__alignof__(struct P4));
	printf("enums %d %d %d %d %d | %d %d | holds %d %d\n", (int)sizeof(enum E1), (int)sizeof(enum E2), (int)sizeof(enum E3), (int)sizeof(enum E4), (int)sizeof(enum E5),
	       (enum E1)-1 > 0, (enum E2)-1 > 0, (int)sizeof(struct holds), (int)__builtin_offsetof(struct holds, f));
	return 0;
}
