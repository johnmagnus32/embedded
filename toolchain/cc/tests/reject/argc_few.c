// error: too few arguments to function 'a3'
/* A short call used to compile: the missing parameter registers came out of the uninitialised argument area
 * (`a3(1)` emitted `str r0,[sp,#0]` / `pop {r0,r1,r2,r3}` / `bl a3`). */
int a3(int, int, int);
int f(void) { return a3(1); }
