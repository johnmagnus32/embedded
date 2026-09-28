// error: truncates
typedef signed char V __attribute__((vector_size(8)));
V f(V c) { return c + 300; }   /* 300 does not fit the lanes (GCC: involves truncation) */
