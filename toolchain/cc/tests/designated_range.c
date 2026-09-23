// expect: 42
// GNU range designator [lo ... hi] = v in a local array init (group_cpus nodemask compound literal), and
// designated array-of-structs with a nested .field designator (fdt_strerror error_table).
struct e { int n; };
static const struct e tab[] = { [1] = { .n = 26 }, [3] = { .n = 2 } };
int main(void)
{
	int c[6] = { [0 ... 5] = 7 };
	return c[0] + c[5] + tab[1].n + tab[3].n;   /* 7 + 7 + 26 + 2 = 42 */
}
