// expect: 42
// A cleanup variable initialized by an inlined call (the kernel's guard(): `T g __cleanup(dtor) = ctor(&lock)`): the
// expansion's own declarations must not wipe the attribute — the lock is released at scope exit.
int held;
static inline __attribute__((always_inline)) int *ctor(int *l) { int *t = l; (*t)++; return t; }
static inline __attribute__((always_inline)) void dtor(int **p) { int *t = *p; if (t) (*t)--; }
int lock_word;
static int use(void) { int *g __attribute__((cleanup(dtor))) = ctor(&lock_word); held = lock_word; return 5; }
int main(void) { int r = use(); r += use(); return r == 10 && held == 1 && lock_word == 0 ? 42 : 1; }
