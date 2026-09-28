// error: cleanup on a global
void f(int *p);
int g __attribute__((cleanup(f)));
