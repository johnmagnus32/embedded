// expect: 0
/* Chained/nested designators `.a.b.c = v` in a global initializer (kernel trace-event structs use
 * `.event.funcs = ...`). Equivalent to `.a = { .b = { .c = v } }`; the leaf is set, everything else 0. */
struct funcs { int a, b; };
struct inner { int x; struct funcs f; };
struct outer { int head; struct inner ev; int tail; };
static struct outer o = {
	.head = 9,
	.ev.f.b = 7,        /* chained: descend ev -> f -> b */
	.tail = 3,
};
/* array leaf via a chain: .m[2] */
struct arrholder { int pre; int m[4]; int post; };
static struct arrholder h = { .pre = 1, .m[2] = 5, .post = 8 };
int main(void) {
	if (o.head != 9) return 1;
	if (o.ev.x != 0) return 2;
	if (o.ev.f.a != 0) return 3;
	if (o.ev.f.b != 7) return 4;
	if (o.tail != 3) return 5;
	if (h.pre != 1) return 6;
	if (h.m[0] != 0 || h.m[1] != 0 || h.m[2] != 5 || h.m[3] != 0) return 7;
	if (h.post != 8) return 8;
	return 0;
}
