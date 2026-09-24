// expect: 0
/* Array of function pointers: `void (*fns[2])(void)` declarator + init with function names. */
static int hit;
static void s0(void) { hit += 1; }
static void s1(void) { hit += 10; }
static void (*fns[2])(void) = { s0, s1 };
int main(void) { fns[0](); fns[1](); return hit == 11 ? 0 : 1; }
