// expect: 42
// Dead-code elimination: an UNUSED static function that references an undefined symbol must be DROPPED
// (otherwise the link fails on the undefined ref). A USED static function must be kept. This is what lets a
// real kernel .o link — GCC drops unused static inlines; without DCE our cc emitted them all.
extern void nonexistent_symbol_xyzzy(void);
static void dead(void) { nonexistent_symbol_xyzzy(); }   /* unreachable -> dropped with its undefined ref */
static int live(int x) { return x + 2; }
int main(void) { return live(40); }
