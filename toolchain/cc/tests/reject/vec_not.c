// error: on a vector
typedef int V __attribute__((vector_size(8)));
V f(V a) { return !a; }
