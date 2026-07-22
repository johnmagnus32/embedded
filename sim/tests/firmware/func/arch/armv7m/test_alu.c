/*
 * test_alu.c — ALU arithmetic and flag tests
 *
 * Tests N, Z, C, V flags for ADD, SUB, CMP and conditional branches.
 */
#include "test.h"

/* Read xPSR flags via MRS */
static inline unsigned int get_flags(void)
{
    unsigned int val;
    __asm volatile("mrs %0, apsr" : "=r"(val));
    return val >> 28; /* top 4 bits: N Z C V */
}

/* Flags bitmask in top nibble of APSR>>28 */
#define F_N 8
#define F_Z 4
#define F_C 2
#define F_V 1

static void test_add_flags(void)
{
    unsigned int f;

    TEST("add_zero");
    __asm volatile("movs r0, #0\n adds r0, #0" ::: "r0", "cc");
    f = get_flags();
    CHECK(f & F_Z);       /* result is zero */
    CHECK(!(f & F_N));    /* not negative */

    TEST("add_negative");
    __asm volatile("ldr r0, =0x7FFFFFFF\n adds r0, #1" ::: "r0", "cc");
    f = get_flags();
    CHECK(f & F_N);       /* result is 0x80000000 = negative */
    CHECK(f & F_V);       /* signed overflow */
    CHECK(!(f & F_C));    /* no unsigned carry */

    TEST("add_carry");
    __asm volatile("ldr r0, =0xFFFFFFFF\n adds r0, #1" ::: "r0", "cc");
    f = get_flags();
    CHECK(f & F_C);       /* unsigned carry */
    CHECK(f & F_Z);       /* result wraps to 0 */
}

static void test_sub_flags(void)
{
    unsigned int f;

    TEST("sub_zero");
    __asm volatile("movs r0, #5\n subs r0, #5" ::: "r0", "cc");
    f = get_flags();
    CHECK(f & F_Z);
    CHECK(f & F_C);       /* ARM: C=1 when no borrow */

    TEST("sub_borrow");
    __asm volatile("movs r0, #0\n subs r0, #1" ::: "r0", "cc");
    f = get_flags();
    CHECK(!(f & F_C));    /* borrow: C=0 */
    CHECK(f & F_N);       /* result is 0xFFFFFFFF = negative */

    TEST("sub_overflow");
    __asm volatile("ldr r0, =0x80000000\n subs r0, #1" ::: "r0", "cc");
    f = get_flags();
    CHECK(f & F_V);       /* signed overflow: MIN_INT - 1 */
    CHECK(!(f & F_N));    /* result is 0x7FFFFFFF = positive */
}

static void test_cmp_branches(void)
{
    volatile int result;

    TEST("beq_taken");
    result = 0;
    __asm volatile(
        "movs r0, #42\n"
        "cmp r0, #42\n"
        "beq 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("bne_taken");
    result = 0;
    __asm volatile(
        "movs r0, #10\n"
        "cmp r0, #20\n"
        "bne 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("bgt_signed");
    result = 0;
    __asm volatile(
        "movs r0, #5\n"
        "cmp r0, #3\n"
        "bgt 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("blt_signed");
    result = 0;
    __asm volatile(
        "mvn r0, #0\n"       /* r0 = -1 (0xFFFFFFFF) */
        "cmp r0, #1\n"
        "blt 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("bhi_unsigned");
    result = 0;
    __asm volatile(
        "ldr r0, =0xFFFFFFFF\n"  /* large unsigned */
        "cmp r0, #1\n"
        "bhi 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);
}

void test_main(void)
{
    test_add_flags();
    test_sub_flags();
    test_cmp_branches();
    TEST_DONE("alu");
}
