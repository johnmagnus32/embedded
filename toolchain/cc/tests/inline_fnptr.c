// expect: 42
// An inlined function-pointer parameter bound to &f: a call through it (by name, or through *) calls f — never a symbol named like the parameter.
struct kref { int n; };
static void my_release(struct kref *k) { k->n = 99; }
static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref)) { if (--kref->n == 0) { release(kref); return 1; } return 0; }
static inline int apply(int (*f)(int), int x) { return (*f)(x) + f(x); }
static int dbl(int x) { return 2 * x; }
int main(void) { struct kref k = { 1 }; int r = kref_put(&k, my_release); return r == 1 && k.n == 99 && apply(dbl, 5) == 20 ? 42 : 1; }
