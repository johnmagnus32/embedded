// error: incompatible vector types in an initializer
typedef int V __attribute__((vector_size(16)));
V g = 1;
