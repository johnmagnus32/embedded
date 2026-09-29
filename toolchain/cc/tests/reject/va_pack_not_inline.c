// error: isn't always_inline
int g(int, ...);
static inline int f(int x, ...) { return g(x, __builtin_va_arg_pack()); }
