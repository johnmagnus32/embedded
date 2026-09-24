// expect: 0
/* A constant-condition ternary selecting a function address, in a global initializer:
 * `(cond) ? fnA : fnB` (kernel debugfs `(false) ? write_signed : write`). */
static int fa(void) { return 1; }
static int fb(void) { return 2; }
static int (*fp)(void) = (1 == 0) ? fa : fb;   /* picks fb */
int main(void) { return fp() == 2 ? 0 : 1; }
