// expect: 42
// __builtin_types_compatible_p(TA, TB) -> 0/1, plus eval_const folding of || and && — used together as
// __builtin_choose_expr conditions in the kernel's abs()/math64 (mul_u64_u64_shr) macros.
int main(void)
{
	int a = __builtin_choose_expr(
		__builtin_types_compatible_p(typeof(1), int) || __builtin_types_compatible_p(typeof(1), long),
		42, 999);
	int b = __builtin_choose_expr(__builtin_types_compatible_p(typeof('x'), char) && 0, 111, 0);
	return a + b;
}
