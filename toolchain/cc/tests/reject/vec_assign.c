// error: incompatible vector types
typedef int V __attribute__((vector_size(8))); typedef unsigned U __attribute__((vector_size(8)));
void f(V *a, U b) { *a = b; }   /* signedness differs: not implicit without -flax-vector-conversions */
