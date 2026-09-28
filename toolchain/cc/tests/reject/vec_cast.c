// error: cannot convert between
typedef int V __attribute__((vector_size(16)));
char f(V a) { return (char)a; }
