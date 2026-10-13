// expect: 23
/* -fPIC addressing, split by whether another module could interpose the symbol (GCC's rule, checked by the
 * Makefile on the -fPIC output): a string literal, a `static` object and a hidden one are addressed
 * PC-relative (R_ARM_REL32); an interposable global and an extern go through the GOT (R_ARM_GOT_PREL).
 * The difference IS the property: a PC-relative address is right the moment the code is loaded, while a GOT
 * slot holds a link-time address until something relocates it — Linux's decompressor calls C code
 * (fdt_check_mem_start) before head.S fixes up .got. The run checks the values. */
__attribute__((visibility("hidden"))) int hidden_var = 9;   /* invisible outside: PC-relative */
int glob_var = 4;                                           /* ours, but interposable: GOT     */
static int counter = 7;                                     /* file-local: PC-relative         */
static const char tbl[4] = { 1, 2, 3, 4 };                  /* file-local: PC-relative         */

const char *s(void) { return "literal"; }                   /* a string literal: PC-relative   */
int g(void) { return glob_var; }
int h(void) { return hidden_var; }
int t(void) { return ++counter + tbl[1]; }                  /* written, so not const-folded     */
int main(void) { return s()[0] - 'l' + g() + h() + t(); }
