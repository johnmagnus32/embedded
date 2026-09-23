// expect: 42
// __builtin_has_attribute(expr, attr): compile-time attribute query (we track none -> 0), used in a
// BUILD_BUG-style sizeof(struct{int:-!!(...);}) in fs.h's super_set_sysfs_name_id().
char buf[8];
int main(void)
{
	int pad = (int)(sizeof(struct { int : (-!!(__builtin_has_attribute(buf, nonstring))); }));
	return 42 + pad;   /* has_attribute -> 0 -> bitfield width 0 -> pad 0 */
}
