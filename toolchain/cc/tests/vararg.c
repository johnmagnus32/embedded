int sum(int n, ...) {
  __builtin_va_list ap; __builtin_va_start(ap, n);
  int s = 0; int i;
  for (i = 0; i < n; i++) s += __builtin_va_arg(ap, int);
  __builtin_va_end(ap);
  return s;
}
int main(void){ return sum(4, 10, 20, 5, 7); }   // expect: 42
