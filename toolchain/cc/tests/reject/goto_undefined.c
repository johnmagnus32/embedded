// error: label 'nowhere' used but not defined
/* cc emitted `b .Ll_f.nowhere` and as then promoted that undefined .L local to an undefined GLOBAL symbol,
 * so the only complaint came from ld, naming a symbol the source never wrote. */
int f(void) { goto nowhere; return 0; }
