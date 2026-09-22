// expect: 42
// Front-end features surfaced by compiling mainline lib/string.c: empty statements, address-constant
// global initializers, and a bare function name used as a value (function designator -> pointer).
static int gv = 40;
int *gp = &gv;                           // address-constant global initializer -> .word gv
static int inc(int x) { return x + 1; }

int main(void) {
	int r = 0, i;
	for (i = 0; i < 3; i++) ;            // empty-statement loop body
	if (i == 3)            r += 2;       //                                 -> 2
	if (*gp == 40)         r += 30;      // gp was initialized to &gv        -> 32
	int (*fp)(int) = inc;               // bare function name = its address
	if (fp(9) == 10)       r += 10;      // call through the function pointer -> 42
	return r;
}
