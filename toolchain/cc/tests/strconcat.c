// expect: 42
// Adjacent string-literal concatenation ("a" "b" -> "ab"), pervasive via the kernel's printk KERN-level
// prefix: _printk("\001" "4" "fmt...", ...). Also needs a string buffer bigger than the old 64 bytes.
static int sl(const char *s) { int n = 0; while (*s++) n++; return n; }

int main(void)
{
	const char *m = "\001" "4" "abc";   /* 1 + 1 + 3 = 5 bytes */
	return sl(m) + 37;                   /* 5 + 37 = 42 */
}
