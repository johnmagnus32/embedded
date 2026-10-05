// expect: 42
// An unsized array is in scope in its own initializer (C: from the end of its declarator): `T a[] = { { &a } }` (pr41163).
struct option { void *value; int n; };
int check(struct option *o);
int main(void) { int x = 5; struct option options[] = { { &options, 1 }, { &x, 2 } }; return options[0].value == (void *)options && options[1].value == &x && sizeof options == 16 ? 42 : 1; }
