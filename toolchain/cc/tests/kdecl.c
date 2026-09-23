// expect: 42
// Kernel-C declaration forms surfaced by the lib/*.c coverage survey:
//  - a local declaration led by __attribute__ and a trailing __attribute__ on a
//    prototype (the READ_ONCE/BUILD_BUG_ON `do{}while(0)` shape), and
//  - an unsized global array `T x[] = {...}` (length inferred from the initializer).
unsigned char tab[] = { 1, 2 | 8, 4 };          /* unsized: values 1,10,4 ; sizeof == 3 */
int side;

static void body(void)
{
	__attribute__((__unused__)) extern void ext(void) __attribute__((__unused__));
	side = 24;
}

int main(void)
{
	body();
	return tab[0] + tab[1] + tab[2] + (int)sizeof(tab) + side;   /* 1+10+4+3+24 = 42 */
}
