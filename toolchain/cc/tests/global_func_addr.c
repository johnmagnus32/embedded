// expect: 0
/* Function pointers in a global initializer: a bare function name (fa) AND &function (fb, which parses as a
 * double ND_ADDR since a function designator is already an address). Both must fold to the symbol; we call
 * through the stored pointers to confirm they point at the right functions. */
static int ca, cb;
static void fa(void) { ca = 11; }
static void fb(void) { cb = 22; }
struct ops { void (*a)(void); void (*b)(void); };
static struct ops o = { .a = fa, .b = &fb };
int main(void) {
	o.a();
	o.b();
	if (ca != 11) return 1;
	if (cb != 22) return 2;
	return 0;
}
