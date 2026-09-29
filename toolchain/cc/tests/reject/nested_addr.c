// error: needs a trampoline
int f(void) { int k = 1; int g(void) { return k; } int (*p)(void) = g; return p(); }
