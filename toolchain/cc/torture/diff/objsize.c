/* objsize.c — __builtin_object_size where the declarations decide it (the kernel's strtomem_pad BUILD_BUG_ONs on it):
 * named arrays and objects, &object, a non-trailing member array (modes 1/3: the member; 0/2: the rest of a named
 * object) — and "unknown" where GCC's answer needs dataflow or a trailing member might be a flexible array. */
int printf(const char *, ...);
struct hd { int id; char vendor[8]; char product[16]; int tail[2]; };
struct hd g;
static char gbuf[40];
int show(struct hd *p, char *q) {
	struct hd l; char a[12];
	printf("%d %d %d %d\n", (int)__builtin_object_size(a, 0), (int)__builtin_object_size(a, 1), (int)__builtin_object_size(a, 2), (int)__builtin_object_size(&l, 0));
	printf("%d %d %d %d\n", (int)__builtin_object_size(gbuf, 0), (int)__builtin_object_size(&g, 1), (int)__builtin_object_size(g.vendor, 1), (int)__builtin_object_size(g.vendor, 0));
	printf("%d %d %d %d\n", (int)__builtin_object_size(p->vendor, 1), (int)__builtin_object_size(p->product, 3), (int)__builtin_object_size(&p->id, 1), (int)__builtin_object_size(l.product, 2));
	printf("%d %d %d %d\n", (int)__builtin_object_size(p->tail, 1), (int)__builtin_object_size(p->vendor, 2), (int)__builtin_object_size(q, 1), (int)__builtin_object_size(q, 2));
	printf("%d %d\n", (int)__builtin_object_size("abcd", 0), (int)__builtin_object_size(&l.vendor, 0));
	return 0;
}
int main(void) { return show(&g, gbuf); }
