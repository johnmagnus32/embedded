// expect: 0
/* An array-typed struct member used as a value in a global init decays to its address (&b.name[0]). */
struct box { char name[8]; };
static struct box b = { "hi" };
static char *p = b.name;
int main(void) {
	if (p != b.name) return 1;
	if (p[0] != 'h' || p[1] != 'i' || p[2] != 0) return 2;
	return 0;
}
