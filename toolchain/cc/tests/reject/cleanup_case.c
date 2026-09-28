// error: a case label in the scope of a cleanup variable
void f(int *p);
int h(int k) { switch (k) { case 0: { int x __attribute__((cleanup(f))) = 1; case 1: return x; } } return 0; }
