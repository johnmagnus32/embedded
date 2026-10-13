// expect: 42
/* A PACKED bit-field is laid out bit by bit, so its declared storage unit need not be aligned and may reach
 * PAST THE END of the object: `packed { unsigned f : 20; }` is 3 bytes with a 4-byte unit. Accessing the
 * whole unit therefore reads one byte beyond — and a read-modify-write STORE writes it, corrupting whatever
 * follows. Each guard below is adjacent to its bit-field object by C's own rules (a member of the same
 * struct, at a known offset), so these checks are not luck about global placement. They cannot catch the
 * out-of-bounds access on their own — the old unit-wide read-modify-write wrote the extra byte back
 * unchanged — so q_wr/q_rd below are shape-checked by ../Makefile as well. GCC uses byte accesses here. */
struct __attribute__((packed)) A { unsigned f : 20; };              /* sizeof 3, unit 4 at byte 0 */
struct __attribute__((packed)) B { char pad; unsigned f : 20; };    /* sizeof 4, unit 4 at byte 1 */
struct __attribute__((packed)) S { char pad; signed   f : 20; };    /* the sign-extending read */
struct                         D { char pad; unsigned f : 20; };    /* NOT packed: the aligned fast path */

struct OA { struct A v; unsigned char guard; };
struct OB { struct B v; unsigned char guard; };
struct OS { struct S v; unsigned char guard; };
struct OD { struct D v; unsigned char guard; };

static struct OA oa = { { 0 }, 0xA5 };
static struct OB ob = { { 0, 0 }, 0x5A };
static struct OS os = { { 0, 0 }, 0x3C };
static struct OD od = { { 0, 0 }, 0xC3 };

/* Also SHAPE-checked by ../Makefile: Q's unit (4 bytes at byte 1) reaches one byte past this 4-byte object,
 * so both accesses must be byte-wise over the field's own three bytes. The old unit-wide read-modify-write
 * read and wrote that extra byte — it wrote the same value back, so a plain neighbour survived, but it is an
 * out-of-bounds unaligned access: it faults with the MMU off or on Device memory, and loses the byte to any
 * volatile/MMIO/other-thread/DMA write landing between the load and the store. A value test cannot see that,
 * which is why the Makefile counts the instructions. */
struct __attribute__((packed)) Q { char pad; unsigned f : 20; };
void q_wr(struct Q *q, unsigned v) { q->f = v; }
unsigned q_rd(struct Q *q) { return q->f; }

int main(void) {
	int r = 0;
	if (sizeof(struct A) != 3 || sizeof(struct B) != 4) return 1;   /* the layout itself must match GCC */

	oa.v.f = 0xFFFFF;                      /* all ones: the widest write */
	if (oa.v.f == 0xFFFFF) r += 6;
	if (oa.guard == 0xA5) r += 6;          /* byte 3, one past a 3-byte object */

	ob.v.f = 0x12345;
	if (ob.v.f == 0x12345) r += 6;
	if (ob.guard == 0x5A) r += 6;          /* byte 4, one past a 4-byte object */
	if (ob.v.pad == 0) r += 6;             /* and the byte BEFORE the unit is untouched */

	os.v.f = -2;                           /* signed: reads back sign-extended */
	if (os.v.f == -2 && os.guard == 0x3C) r += 6;

	od.v.f = 0x7FFFF;                      /* the unpacked path must still work */
	if (od.v.f == 0x7FFFF && od.guard == 0xC3) r += 6;

	return r;                              /* 7 * 6 = 42 */
}
