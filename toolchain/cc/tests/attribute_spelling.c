// expect: 7
/* GCC accepts `__attribute` (no trailing __) as well as `__attribute__` — kernel noinstr uses it. */
static int __attribute((noinline)) __attribute__((unused)) foo(void) { return 7; }
int main(void) { return foo(); }
