// error: where a scalar is required
typedef int V __attribute__((vector_size(8)));
int f(V a) { if (a) return 1; return 0; }
