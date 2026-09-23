// expect: 42
// GNU binary conditional (elvis) `a ?: b` == `a ? a : b` — kernel uaccess.h: `return ret ?: -EFAULT;`.
int main(void)
{
	int a = 0, b = 42, c = 7;
	return (a ?: b) - (c ?: 99) + 7;   /* 42 - 7 + 7 = 42 */
}
