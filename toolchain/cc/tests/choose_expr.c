// expect: 42
// __builtin_choose_expr(cond, a, b): compile-time arm selection by a constant cond; the unchosen arm is
// discarded (kernel GENMASK_INPUT_CHECK / find.h BUILD_BUG_ON_ZERO wrap this so an arm is always constant).
int main(void)
{
	int x = __builtin_choose_expr(sizeof(int) == 4, 42, 999);   /* cond true  -> 42 */
	int y = __builtin_choose_expr(sizeof(char) == 4, 111, 0);   /* cond false -> 0  */
	return x + y;
}
