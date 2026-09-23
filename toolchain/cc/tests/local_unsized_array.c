// expect: 42
// Unsized LOCAL array `T x[] = {...}` — length must be inferred BEFORE frame allocation, else the slot is
// 0 bytes and the init overflows (kernel device.h: const struct attribute_group *groups[] = { grp, NULL };).
int main(void)
{
	int x = 10, y = 32;
	int *a[] = { &x, &y, ((void *)0) };
	return *a[0] + *a[1];   /* 10 + 32 = 42 */
}
