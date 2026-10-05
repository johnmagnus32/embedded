// error: it returns twice
// setjmp() under an operand already on the stack: its second return would find that operand overwritten.
int setjmp(int *b);
int g(void);
int f(int *b) { return g() + setjmp(b); }
