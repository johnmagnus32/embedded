// expect: 42
/* const data placement (GCC's rule, checked by the Makefile on the -fPIC output): const + not volatile -> .rodata;
 * under -fPIC one holding addresses -> .data.rel.ro; volatile const / non-const -> .data. The run checks the values. */
const int k = 5;
const char *const tab[] = { "a", "bc" };
static int twice(int x) { return 2 * x; }
int (*const fns[])(int) = { twice };                        /* a grouped `(*const ...)`: the pointers are const */
const volatile int cv = 7;
int w = 3;
int main(void) { return k + tab[1][1] - 'c' + fns[0](10) + cv + w + (sizeof fns == 4 ? 7 : 0); }
