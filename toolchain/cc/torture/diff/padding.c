/* padding.c — __builtin_clear_padding: the padding between and after members, unused bit-field bits, nested structs,
 * arrays of structs, unions (bits no member uses), plus __builtin_convertvector. Must match GCC. */
int printf(const char *, ...);
struct A { char c; int i; short s; };
struct B { unsigned a : 3, : 2, b : 5; char t; long long w; };
struct C { struct A x[2]; char y; };
union U { char c; short s; struct { char p; int q : 4; } bf; };
typedef int v4si __attribute__((vector_size(16)));
typedef float v4sf __attribute__((vector_size(16)));
static void dump(const char *t, const void *p, int n) { const unsigned char *b = p; printf("%s:", t); for (int i = 0; i < n; i++) printf(" %02x", b[i]); printf("\n"); }
#define FILL(x) __builtin_memset(&(x), 0xa5, sizeof (x))
int main(void) {
	struct A a; FILL(a); a.c = 1; a.i = 2; a.s = 3; __builtin_clear_padding(&a); dump("A", &a, sizeof a);
	struct B b; FILL(b); b.a = 5; b.b = 17; b.t = 9; b.w = 7; __builtin_clear_padding(&b); dump("B", &b, sizeof b);
	struct C c; FILL(c); c.x[0].c = 1; c.x[1].s = 2; c.y = 3; __builtin_clear_padding(&c); dump("C", &c, sizeof c);
	union U u; FILL(u); u.bf.q = -2; __builtin_clear_padding(&u); dump("U", &u, sizeof u);
	v4si v = { 1, -2, 300, 7 }; v4sf f = __builtin_convertvector(v, v4sf); v4si back = __builtin_convertvector(f * 1.5f, v4si);
	dump("cv", &back, sizeof back);
	return 0;
}
