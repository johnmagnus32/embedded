// error: floating vector
typedef float V __attribute__((vector_size(16)));
V f(V a, V b) { return a % b; }
