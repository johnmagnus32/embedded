// expect: 31
/* Block scope: an inner declaration shadows and then ENDS at its `}`; block-scope `static` is one object
 * initialized once (not per call); block-scope `extern` names the global; &&label tables in static data
 * and computed `goto *p`. */
int g = 5;
static int counter(void) { static int n = 10; return n++; }
static int hexdig(unsigned a) { static char hex[] = "0123456789abcdef"; return hex[a]; }
static int dispatch(int i) {
	static void *tbl[] = { &&zero, &&one };
	goto *tbl[i];
zero: return 100;
one:  return 200;
}
int main(void) {
	int r = 0, x = 1;
	{ int x = 2; r += x == 2; }                          /* 1 */
	r += (x == 1) << 1;                                  /* 2: the inner x is gone */
	counter(); counter(); r += (counter() == 12) << 2;   /* 4 */
	{ int g = 9; { extern int g; r += (g == 5) << 3; } (void)g; }   /* 8 */
	r += (hexdig(12) == 'c' && dispatch(0) == 100 && dispatch(1) == 200) << 4;   /* 16 */
	return r;
}
