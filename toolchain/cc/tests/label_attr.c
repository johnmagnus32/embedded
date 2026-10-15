// expect: 42
/* A LABEL ATTRIBUTE is a GNU extension and applies to the LABEL, not to the statement after it. Leaving it
 * unconsumed made the next statement parse as a DECLARATION: `out: __attribute__((unused)) sink(c);` became a
 * declaration of sink with an identifier-list parameter, and the CALL WAS SILENTLY DROPPED — gcc emitted
 * `bl sink` and we emitted nothing. fs/ext4/super.c:5632 does exactly this, which is how it was found: the
 * kernel would not compile once -Wimplicit-int existed to complain about the bogus declaration.
 *
 * typeof(&f) is a POINTER to f's type, where typeof(f) is the function type itself. Both are the same node
 * here because naming a function already decays to its address, so only the written `&` tells them apart.
 * Getting it wrong made `typeof(&f) x;` declare a FUNCTION rather than a variable, so no local existed and
 * the name became an undefined global — vmlinux failed to link with "undefined symbol __result", from
 * asm/fncpy.h's `typeof(funcp) __result;`. */
static int total;
static void add(int v) { total += v; }
extern void target(void);
void target(void) { }   /* the address is taken and compared, so it must exist */

static int labelled(int c)
{
	if (c) goto out;
	return 0;
out: __attribute__((unused))
	add(20);               /* a CALL, not a declaration */
	return 1;
}
int main(void)
{
	if (!labelled(1)) return 1;
	int a = sizeof(typeof(&target));      /* a pointer: 4 */
	typeof(&target) p = target;           /* a VARIABLE of pointer type, not a function declaration */
	if (p != target) return 2;
	add(a * 4 + 6);        /* 4*4 + 6 = 22 */
	return total;          /* 20 + 22 = 42 */
}
