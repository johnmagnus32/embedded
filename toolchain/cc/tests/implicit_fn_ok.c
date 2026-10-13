// expect: 42
/* A builtin needs no declaration (the compiler predeclares it), and a properly declared call is fine. */
int declared(int x);
int declared(int x) { return x; }
int main(void) {
	char *p = __builtin_alloca(8);          /* a builtin: no declaration anywhere, and that is legal */
	p[0] = 40;
	return declared(p[0]) + (__builtin_constant_p(1) ? 2 : 0);
}
