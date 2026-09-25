// expect: 0
/* switch on a constant keeps only the selected case (kernel `switch (sizeof(x))` accessors: the dead cases'
 * asm is invalid for this size and must not be emitted). Fallthrough into the next case and `break` still work;
 * a dead case that references an undefined function must vanish (link would fail otherwise). */
extern void never_defined(void);
int main(void) {
	int r = 0;
	switch (sizeof(int)) { case 1: never_defined(); break; case 4: r += 1; break; case 8: never_defined(); break; default: never_defined(); }
	switch (2) { case 1: never_defined(); case 2: r += 10; case 3: case 7: r += 100; break; case 4: never_defined(); }   /* fallthrough 2->3->7: each kept case needs its own label */
	switch (99) { case 1: never_defined(); break; default: r += 1000; }
	switch (5) { case 1: never_defined(); break; }                                    /* nothing matches */
	for (int i = 0; i < 3; i++) switch (4) { case 4: r += 10000; break; case 5: never_defined(); }   /* break exits switch, not loop */
	return r == 31111 ? 0 : 1;
}
