// error: different lane counts
typedef int V2 __attribute__((vector_size(8))); typedef short V4 __attribute__((vector_size(8)));
V2 f(V2 a, V4 b) { return a + b; }
