// expect: 42
// Dereferencing the result of a function that returns a pointer requires the callee's
// return type to be recorded in the func-signature table (kernel: *preempt_count_ptr()).
int store;
int *slot(void) { return &store; }

int main(void)
{
	*slot() = 42;
	return *slot();
}
