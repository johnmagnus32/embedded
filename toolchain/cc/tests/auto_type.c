// expect: 42
// GNU __auto_type: the variable's type is inferred from its initializer. Pervasive in the kernel's
// min()/max() (__careful_cmp) macros: __auto_type __x = (a); __auto_type __y = (b); ...
int main(void)
{
	int d = 13;
	__auto_type x = (0L);
	__auto_type y = (long)d;
	return (int)(x > y ? x : y) * 2 + 16;   /* max(0,13)=13 -> 13*2+16 = 42 */
}
