// expect: 7
/* symbol visibility: __attribute__((visibility)) -> .hidden/.internal/.protected (checked in the Makefile). */
__attribute__((visibility("hidden"))) int hidden_fn(void) { return 3; }
__attribute__((visibility("protected"))) int prot_var = 4;
extern int hidden_ext __attribute__((visibility("hidden")));
int main(void) { return hidden_fn() + prot_var; }
