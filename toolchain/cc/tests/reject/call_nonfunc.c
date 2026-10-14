// error: called object is not a function or function pointer
/* This used to load the integer's VALUE and branch to it: `ldr r0,[r0]` then `blx r12`. A LOCAL int, because
 * the five named-operand paths build their ND_CALL away from the postfix-call site. */
int f(void) { int notafunc = 3; return notafunc(1, 2); }
