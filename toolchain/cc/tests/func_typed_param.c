// expect: 5
/* A function-typed parameter `int op(int)` adjusts to a function pointer (kernel bpf_map_ops inline helpers). */
static int apply(int x, int op(int)) { return op(x); }
static int inc(int a) { return a + 1; }
int main(void) { return apply(4, inc); }
