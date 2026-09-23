// expect: 42
// A call applied to a non-identifier expression: _Generic(...) selects a function, then it is called
// (kernel seqlock's __seqprop(...)(&sl->seqcount)). Exercises the general postfix '(' call suffix.
static int dbl(int a) { return a * 2; }
static int inc(int a) { return a + 1; }

int main(void)
{
	int x = 0;
	return _Generic(x, int: dbl, default: inc)(21);   /* picks dbl -> 21*2 = 42 */
}
