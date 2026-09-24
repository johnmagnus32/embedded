// expect: 0
/* Two global-initializer forms the kernel crypto core uses that the old scalar path rejected:
 *  - a compound literal `(T){...}` as an initializer value (spinlock/rwsem macros)
 *  - `&gvar.member` self-references (LIST_HEAD_INIT), which must emit `symbol + byte-offset`.
 * Both are read back at runtime, so a wrong addend or a botched `.word sym+N` reloc fails the test. */

struct list_head { struct list_head *next, *prev; };
struct thing { int pad; struct list_head list; };   /* .list is at a NONZERO offset -> exercises the addend */
static struct thing t = { .pad = 5, .list = { &t.list, &t.list } };

struct pt { int x, y; };
struct box { int id; struct pt origin; };
static struct box b = { .id = 7, .origin = (struct pt){ .y = 2, .x = 1 } };   /* out-of-order compound literal */

int main(void) {
	if (t.pad != 5) return 1;
	if (t.list.next != &t.list) return 2;
	if (t.list.prev != &t.list) return 3;
	if (b.id != 7) return 4;
	if (b.origin.x != 1) return 5;
	if (b.origin.y != 2) return 6;
	return 0;
}
