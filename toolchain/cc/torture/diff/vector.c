/* vector.c — GCC generic vectors (vector_size): every operator over every element type, scalar broadcast on either
 * side, comparisons (-1/0 lanes, opaque), shifts by vector and scalar counts, op=/++/--, lanes as lvalues (constant
 * and runtime index), casts, __builtin_shuffle, initializers (partial, elided braces, globals, arrays, compound
 * literals), layout, ?:, calls through pointers. Prints the raw lane bytes (must match GCC). */
int printf(const char *, ...);
typedef signed char v16qi __attribute__((vector_size(16)));
typedef unsigned char v8uqi __attribute__((vector_size(8)));
typedef short v8hi __attribute__((vector_size(16)));
typedef unsigned short v4uhi __attribute__((vector_size(8)));
typedef int v4si __attribute__((vector_size(16)));
typedef unsigned v2usi __attribute__((vector_size(8)));
typedef long long v2di __attribute__((vector_size(16)));
typedef unsigned long long v4udi __attribute__((vector_size(32)));
typedef float v4sf __attribute__((vector_size(16)));
typedef double v2df __attribute__((vector_size(16)));
typedef int v2si __attribute__((vector_size(8)));
typedef short v2hi __attribute__((vector_size(4)));
typedef __attribute__((vector_size(8))) unsigned char v8uqi_b;   /* the attribute before the type */
typedef int word __attribute__((mode(word)));
typedef unsigned u8m __attribute__((mode(QI)));

static void dump(const char *tag, const void *p, int n) {
	const unsigned char *b = p; printf("%s:", tag);
	for (int i = 0; i < n; i++) printf(" %02x", b[i]);
	printf("\n");
}
#define D(tag, v) dump(tag, &(v), sizeof (v))

#define INT_OPS(T, a, b, s) do { T r; \
	r = a + b; D(#T " +", r); r = a - b; D(#T " -", r); r = a * b; D(#T " *", r); r = a / b; D(#T " /", r); \
	r = a % b; D(#T " %", r); r = a & b; D(#T " &", r); r = a | b; D(#T " |", r); r = a ^ b; D(#T " ^", r); \
	r = a << (b & 3); D(#T " <<v", r); r = a >> (b & 3); D(#T " >>v", r); r = a << s; D(#T " <<s", r); r = a >> s; D(#T " >>s", r); \
	r = -a; D(#T " neg", r); r = ~a; D(#T " not", r); r = +a; D(#T " pos", r); \
	r = a + 2; D(#T " +2", r); r = 2 - a; D(#T " 2-", r); r = 7 * a; D(#T " 7*", r); r = a / 3; D(#T " /3", r); r = 100 % b; D(#T " 100%", r); \
	} while (0)
#define CMP_OPS(T, a, b) do { \
	__typeof__(a < b) c; c = a < b; D(#T " <", c); c = a <= b; D(#T " <=", c); c = a > b; D(#T " >", c); \
	c = a >= b; D(#T " >=", c); c = a == b; D(#T " ==", c); c = a != b; D(#T " !=", c); c = a < 2; D(#T " <2", c); c = 1 == a; D(#T " 1==", c); \
	} while (0)
#define FP_OPS(T, a, b) do { T r; \
	r = a + b; D(#T " +", r); r = a - b; D(#T " -", r); r = a * b; D(#T " *", r); r = a / b; D(#T " /", r); r = -a; D(#T " neg", r); \
	r = a + 2; D(#T " +2", r); r = 0.5f * a; D(#T " .5*", r); r = 3 - a; D(#T " 3-", r); \
	} while (0)

struct lay { char c; v4si v; short s; v2hi h; };
struct elide { v4si v; int x; };
v4si gv = { 1, -2, 3 };
v4si garr[2] = { { 1, 2 }, (v4si){ 5, 6, 7, 8 } };
struct elide gel = { 1, 2, 3, 4, 5 };
v8uqi_b gb = { 250, 251, 252, 253, 254, 255, 0, 1 };
static v4si add1(v4si a) { return a + 1; }
static v2si pick(int c, v2si a, v2si b) { return c ? a : b; }

int main(void) {
	v16qi qa = { 1, -2, 3, -4, 5, -6, 7, -8, 100, -100, 127, -128, 0, 1, 2, 3 }, qb = { 3, 3, -3, -3, 7, 7, 7, 7, 3, 3, 5, 5, 1, 1, 1, 1 };
	v8uqi ua = { 1, 2, 250, 251, 128, 7, 0, 255 }, ub = { 3, 3, 3, 3, 3, 3, 3, 3 };
	v8hi ha = { 1, -2, 300, -400, 32767, -32768, 12, 13 }, hb = { 7, 7, -7, -7, 3, 3, 5, 5 };
	v4uhi uha = { 1, 65535, 40000, 3 }, uhb = { 2, 2, 3, 3 };
	v4si ia = { 100, -200, 0x7fffffff, -7 }, ib = { 3, 7, 2, -2 };
	v2usi uia = { 0xffffffffu, 5 }, uib = { 3, 2 };
	v2di la = { 0x123456789LL, -5 }, lb = { 7, 3 };
	v4udi ula = { ~0ULL, 1, 0x8000000000000000ULL, 12345 }, ulb = { 3, 1, 5, 7 };
	int s = 1; unsigned char sc = 2;
	INT_OPS(v16qi, qa, qb, s); INT_OPS(v8uqi, ua, ub, sc); INT_OPS(v8hi, ha, hb, s); INT_OPS(v4uhi, uha, uhb, s);
	INT_OPS(v4si, ia, ib, s); INT_OPS(v2usi, uia, uib, s); INT_OPS(v2di, la, lb, s); INT_OPS(v4udi, ula, ulb, s);
	CMP_OPS(v16qi, qa, qb); CMP_OPS(v8uqi, ua, ub); CMP_OPS(v8hi, ha, hb); CMP_OPS(v4si, ia, ib); CMP_OPS(v2usi, uia, uib); CMP_OPS(v2di, la, lb);
	v4sf fa = { 1.5f, -2.25f, 1e20f, 3 }, fb = { 2, 4, -1e-3f, 0.5f };
	v2df da = { 1.0 / 3, -1e300 }, db = { 3, 7 };
	FP_OPS(v4sf, fa, fb); FP_OPS(v2df, da, db); CMP_OPS(v4sf, fa, fb); CMP_OPS(v2df, da, db);

	/* op=, ++/--, their values */
	v4si m = { 1, 2, 3, 4 }, old;
	m += ib; D("+=", m); m *= 3; D("*=", m); m <<= 2; D("<<=", m); m >>= ib & 1; D(">>=", m); m %= 7; D("%=", m);
	old = m++; D("post", old); D("post m", m); old = --m; D("pre", old); v4sf fm = fa; fm++; D("f++", fm); fm /= 2; D("f/=", fm);
	v4si *pm = &m; int k = 0; pm[k++] -= 1; D("p-=", m); printf("k %d\n", k);

	/* lanes */
	int i = 2; m[0] = 42; m[i] = -m[i + 1]; D("lanes", m); printf("m %d %d %d %d\n", m[0], m[1], m[i], m[3]);
	printf("rv %d %d\n", add1(m)[1], ((v4si){ 9, 8, 7, 6 })[i]); int *ep = &m[3]; *ep = 5; D("&lane", m);
	printf("ga %d %d %d\n", garr[1][2], gv[1], gel.v[3] + gel.x);

	/* casts */
	v2si c2 = (v2si)la[0] < (v2si)0LL;
	long long ll = (long long)(v2si){ 7, -1 }; printf("ll %llx\n", ll);
	v8hi rh = (v8hi)ia; D("v4si->v8hi", rh); v4sf rf = (v4sf)ia; D("v4si->v4sf", rf); v4si ri = (v4si)fa; D("v4sf->v4si", ri);
	int w = (int)(v2hi){ -1, 2 }; printf("w %x\n", w); v2hi h2 = (v2hi)0x12345678; D("int->v2hi", h2); D("c2", c2);
	(void)m;

	/* shuffles */
	v4si msk = { 3, 0, 5, 2 }, sh1 = __builtin_shuffle(ia, msk), sh2 = __builtin_shuffle(ia, ib, msk); D("sh1", sh1); D("sh2", sh2);
	v2df shd = __builtin_shuffle(da, (v2di){ 1, 0 }); D("shd", shd);

	/* initializers and layout */
	v4si part = { 9 }; D("part", part); v4si empty = {}; D("empty", empty); D("gv", gv); D("garr", garr); D("gel", gel); D("gb", gb);
	static v8hi st = { 1, 2, 3 }; st += 1; D("static", st);
	struct elide lel = { 7, 8, 9, 10, 11 }; D("lel", lel);
	struct lay L = { 1, { 2, 3 }, 4, { 5, 6 } };
	printf("lay %d %d %d %d %d %d\n", (int)sizeof L, (int)__alignof__(struct lay), (int)((char *)&L.v - (char *)&L), (int)((char *)&L.s - (char *)&L),
	       (int)((char *)&L.h - (char *)&L), (int)__alignof__(v2hi));
	printf("sizes %d %d %d %d %d\n", (int)sizeof(v16qi), (int)sizeof(v4udi), (int)__alignof__(v4udi), (int)__alignof__(v2hi), (int)sizeof(word) + (int)sizeof(u8m) * 10);
	printf("class %d\n", __builtin_classify_type(ia));

	/* ?:, comma, calls through a pointer, opaque comparison results */
	v2si pa = { 1, 2 }, pb = { 3, 4 }, (*fp)(int, v2si, v2si) = pick;
	v2si t = fp(0, pa, pb); D("pick", t); t = (s ? pa : pb); D("?:", t); t = (pa, pb); D(",", t);
	v4sf asf = ia < ib; D("opaque->v4sf", asf); v8hi ah = ia == ia; D("opaque->v8hi", ah);
	v4si st2 = ({ v4si q = ia; q[1] = 0; q; }); D("stmtexpr", st2);
	return 0;
}
