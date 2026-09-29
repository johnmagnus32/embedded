// error: a static local in always_inline
int g(int, ...);
static inline __attribute__((always_inline)) int f(int x, ...) { static int n; n++; return g(n, __builtin_va_arg_pack()); }
int h(void) { return f(2, 1); }
