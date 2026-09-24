// expect: 42
/* File-scope basic asm: cc concatenates + escape-decodes the strings and emits them verbatim; our as
 * handles `.weak` and `.set name, target` (symbol aliasing). This is the kernel COND_SYSCALL pattern:
 * an unimplemented syscall is weak-aliased to a fallback. Here myalias -> real_impl, called via a
 * separate prototype so the alias must resolve at link time. */
int real_impl(void) { return 42; }
asm(".weak myalias\n.set myalias, real_impl");
int myalias(void);
int main(void) { return myalias(); }
