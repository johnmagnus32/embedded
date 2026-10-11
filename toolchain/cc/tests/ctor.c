// expect: 42
/* __attribute__((constructor)) runs before main: cc puts a word pointing at each one in .init_array
 * (priority order: .init_array.00101 sorts before the bare .init_array), the linker collects them between
 * __init_array_start/_end, and crt.s walks them. */
static int sum;
__attribute__((constructor(101))) static void first(void) { sum = 1; }
__attribute__((constructor)) static void second(void) { sum = sum * 10 + 2; }
__attribute__((destructor)) static void never_runs(void) { sum = 999; }   /* nothing calls .fini_array here */
int main(void) { return sum == 12 ? 42 : sum; }
