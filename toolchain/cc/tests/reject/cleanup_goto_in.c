// error: jumps into the scope of a cleanup variable
void f(int *p);
int h(int k) { if (k) goto in; { int x __attribute__((cleanup(f))) = 1; in: return x; } }
