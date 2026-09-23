// expect: 42
// __alignof__/_Alignof/alignof on a type (surfaced by kstrtox.h: __alignof__(unsigned long)).
int main(void)
{
	return __alignof__(long long) * 4    /* 8*4 = 32 */
	     + _Alignof(int) * 2             /* 4*2 = 8  */
	     + alignof(char) * 2;            /* 1*2 = 2  */
}
