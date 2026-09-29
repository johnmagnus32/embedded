// error: recursive always_inline
int g(int, ...);
static inline __attribute__((always_inline)) int f(int x, ...) { return x ? f(x - 1, __builtin_va_arg_pack()) : g(0, __builtin_va_arg_pack()); }
int h(void) { return f(2, 1); }
