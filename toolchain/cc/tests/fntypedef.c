// expect: 42
// Function-type typedef `typedef R name(params);` (no '*'), used via pointer — kernel
// signal-defs.h: typedef void __signalfn_t(int); typedef __signalfn_t *__sighandler_t;
typedef void sigfn_t(int);
typedef sigfn_t *sighandler_t;
static int hit;
static void h(int x) { hit = x; }
int main(void)
{
	sighandler_t p = h;
	p(42);
	return hit;
}
