// expect: 42
/* A compound literal in an initializer is an object of ITS OWN type. When that type is the destination's,
 * the braces may be written straight into the destination; when it is not, the literal is a separate
 * anonymous object and what the destination gets is its value — for an array, its decayed ADDRESS.
 *
 * Linux's CLK_HW_INIT_HW builds a clock's parent list exactly this way:
 *   .parent_hws = (const struct clk_hw *[]){ _parent }
 * a pointer member initialized from an array literal. Writing those braces into the member instead stores
 * element [0] and drops the rest, which on T113 silicon handed the clock framework a clk_core where it
 * wanted a clk_hw and oopsed in __clk_core_init. */
struct S { int a; int b; };
static struct S x = { 1, 2 };
static struct S y = { 10, 20 };

/* diverted: pointer destination, array literal -> an anonymous array, destination holds its address */
static struct S *const *two = (struct S *const[]){ &x, &y };
struct Outer { struct S *const *p; int n; };
static struct Outer o1 = { .p = (struct S *const[]){ &x, &y }, .n = 2 };   /* designated   */
static struct Outer o2 = { (struct S *const[]){ &y, &x }, 2 };             /* positional   */
static struct Outer o3 = { .p = ((struct S *const[]){ &y, &x }), .n = 2 }; /* grouped ()   */
static const int *ints = (const int[]){ 3, 5, 7 };

/* in place: the literal IS an object of the destination's type, so the braces land directly */
static struct S same = (struct S){ 4, 5 };
static int scalar = (int){ 6 };

int main(void) {
	int r = 0;
	r += two[0]->a + two[1]->a;          /*  1 + 10 = 11 */
	r += o1.p[1]->b;                     /*       20 -> 31 */
	r += o2.p[0]->a - o2.p[1]->a;        /*   10 - 1 =  9 -> 40 */
	r += o3.p[0]->a == 10 ? 1 : 0;       /*            1 -> 41 */
	r += ints[0] + ints[1] + ints[2];    /*  3+5+7 = 15 -> 56 */
	r += same.a + same.b;                /*    4+5 =  9 -> 65 */
	r += scalar;                         /*            6 -> 71 */
	r -= o1.n + o2.n + o3.n;             /*       -6   -> 65 */
	return r - 23;                       /*           42 */
}
