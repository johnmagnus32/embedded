// expect: 0
/* Global struct initializers with out-of-order designators, gaps, a nested struct whose designators are
 * also out of member order (the kernel crypto pattern that broke the old streaming cursor), and a
 * last-writer-wins override. Exercises global_init's slot model. */

struct inner { int a, b, c, d, e, f; };
struct outer {
	int first;
	int skipped;          /* left uninitialized -> must read back 0 */
	struct inner base;
	int last;
};

/* .base's designators are NOT in member-definition order, and the first one lands on the LAST member (.f),
 * which used to make the member cursor go NULL and drop everything after it. */
static const struct outer o = {
	.first = 11,
	.base = { .f = 6, .a = 1, .c = 3 },   /* b,d,e stay 0 */
	.last = 99,
};

/* last writer wins: .x is written twice; the second value must win. */
struct pair { int x, y; };
static const struct pair p = { .x = 1, .y = 2, .x = 7 };

int main(void) {
	if (o.first != 11) return 1;
	if (o.skipped != 0) return 2;
	if (o.base.a != 1) return 3;
	if (o.base.b != 0) return 4;
	if (o.base.c != 3) return 5;
	if (o.base.d != 0) return 6;
	if (o.base.e != 0) return 7;
	if (o.base.f != 6) return 8;
	if (o.last != 99) return 9;
	if (p.x != 7) return 10;
	if (p.y != 2) return 11;
	return 0;
}
