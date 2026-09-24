// expect: 42
/* File-scope basic asm: cc concatenates + escape-decodes the strings and emits them verbatim (kernel
 * COND_SYSCALL uses this for .weak/.set aliases; here we use directives our `as` already supports). */
asm(".data\n.globl asm_val\nasm_val: .word 42");
extern int asm_val;
int main(void) { return asm_val; }
