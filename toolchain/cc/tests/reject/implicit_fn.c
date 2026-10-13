// error: implicit declaration of function 'not_declared_anywhere'
/* C99 removed the implicit `int f()`. We used to turn an undeclared call into a direct `bl` with no
 * diagnostic, which is how a call to a function that did not exist reached the LINKER instead of the
 * compiler — and why the kernel's own -Werror=implicit-function-declaration never protected these builds.
 * -Wno-implicit-function-declaration restores the old behaviour; __builtin_* stays exempt, as in GCC. */
int f(void) { return not_declared_anywhere(3); }
