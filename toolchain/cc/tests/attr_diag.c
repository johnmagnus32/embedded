// expect: 42
/* __attribute__((error)) / ((warning)) are raised where the call is EMITTED, not where it is written — so a
 * call in a branch the constant fold removes is silent. That deferral is the whole mechanism behind the
 * kernel's BUILD_BUG_ON: a guarded call to a function declared __error__, 4210 sites of it. Both attributes
 * used to be dropped as "no effect on generated code", which made every one of those assertions a no-op.
 *
 * Everything here must COMPILE: each error-attribute call sits in a branch that folds away. ../Makefile
 * additionally checks that the warning fires exactly once, and reject/attr_error.c covers the live call. */
extern void boom(void) __attribute__((__error__("must not be emitted")));   /* never emitted, so never linked */
extern void nag(void) __attribute__((__warning__("emitted on purpose")));
void nag(void) { }   /* this one IS called, so it has to exist: a warning attribute does not stop the call */

static int guarded(int sz) {
	if (!(sizeof(int) == 4)) boom();            /* the BUILD_BUG_ON shape: the guard folds to false */
	if (0) boom();                               /* a literally dead branch */
	if (__builtin_constant_p(1) && 0) boom();    /* and one behind a builtin that folds */
	return sz;
}
int main(void) {
	int r = guarded(20);
	if (sizeof(long) == 4) r += 20; else boom(); /* the taken side is kept, the else folds away */
	nag();                                        /* survives: must warn, must still compile */
	return r + 2;                                 /* 20 + 20 + 2 = 42 */
}
