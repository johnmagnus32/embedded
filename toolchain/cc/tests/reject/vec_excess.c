// error: excess elements
typedef int V __attribute__((vector_size(8)));
V g = { 1, 2, 3 };
