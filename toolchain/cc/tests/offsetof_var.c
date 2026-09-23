// expect: 42
// __builtin_offsetof with a RUNTIME array index — kernel container_of(ptr, type, member[idx]) (rbtree_latch).
// offsetof stays a runtime value offsetof(t,arr) + idx*elemsize instead of requiring a constant index.
struct T { int hdr; int arr[8]; };
int main(void)
{
	int i = 3;
	int c = (int)__builtin_offsetof(struct T, arr[i]);   /* 4 + 3*4 = 16 */
	int k = (int)__builtin_offsetof(struct T, arr[2]);   /* 4 + 2*4 = 12 (constant) */
	return c + k + 14;                                   /* 16 + 12 + 14 = 42 */
}
