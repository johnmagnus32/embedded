// expect: 0
/* A constant-false `if` drops its branch entirely (kernel BUILD_BUG_ON: `if (!(1)) __compiletime_assert();`),
 * so the never-defined callee is never referenced and the link succeeds. A label in a dead branch keeps it. */
extern void never_defined(void);
int main(void) {
	int r = 0;
	if (0) never_defined();                 /* must vanish -> no undefined ref */
	if (!(sizeof(int) == 4)) never_defined();
	if (1) r += 1; else never_defined();
	if (0) { never_defined(); } else r += 2;
	goto in;
	if (0) { in: r += 4; }                  /* label inside -> NOT folded, goto still works */
	return r == 7 ? 0 : 1;
}
