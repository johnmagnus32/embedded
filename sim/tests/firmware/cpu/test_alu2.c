/*
 * test_alu2.c — ADC, SBC, ROR, RSB/NEG, TST, ADR, REV
 */
#include "test.h"

#define F_N 8
#define F_Z 4
#define F_C 2
#define F_V 1

static inline unsigned int get_flags(void)
{
    unsigned int val;
    __asm volatile("mrs %0, apsr" : "=r"(val));
    return val >> 28;
}

static void test_adc(void)
{
    unsigned int out;

    TEST("adc_no_carry");
    /* Clear carry, then ADC r0, r1: should be r0 + r1 + 0 */
    __asm volatile(
        "movs r0, #10\n"
        "movs r1, #20\n"
        "adds r2, r0, #0\n"  /* clears C */
        "adcs r0, r1\n"
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "r2", "cc"
    );
    CHECK(out == 30);

    TEST("adc_with_carry");
    /* Set carry via 0xFFFFFFFF + 1, then ADC */
    __asm volatile(
        "ldr r0, =0xFFFFFFFF\n"
        "adds r0, #1\n"       /* C=1, r0=0 */
        "movs r0, #10\n"
        "movs r1, #20\n"
        "adcs r0, r1\n"       /* 10 + 20 + 1 = 31 */
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "cc"
    );
    CHECK(out == 31);
}

static void test_sbc(void)
{
    unsigned int out;

    TEST("sbc_no_borrow");
    /* Set carry (no borrow): SBC = r0 - r1 - !C = r0 - r1 */
    __asm volatile(
        "movs r0, #50\n"
        "movs r1, #20\n"
        "movs r2, #0\n"
        "adds r2, #0\n"       /* doesn't reliably set C */
        "cmp r0, r1\n"        /* 50 >= 20 → C=1 */
        "sbcs r0, r1\n"       /* 50 - 20 - 0 = 30 */
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "r2", "cc"
    );
    CHECK(out == 30);

    TEST("sbc_with_borrow");
    /* Clear carry (borrow): SBC = r0 - r1 - 1 */
    __asm volatile(
        "movs r0, #0\n"
        "subs r0, #1\n"       /* C=0 (borrow) */
        "movs r0, #50\n"
        "movs r1, #20\n"
        "sbcs r0, r1\n"       /* 50 - 20 - 1 = 29 */
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "cc"
    );
    CHECK(out == 29);
}

static void test_ror(void)
{
    unsigned int out;

    TEST("ror_reg");
    __asm volatile(
        "ldr r0, =0x80000001\n"
        "movs r1, #1\n"
        "rors r0, r1\n"
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "cc"
    );
    CHECK(out == 0xC0000000);  /* bit 0 rotates to bit 31 */

    TEST("ror_by_8");
    __asm volatile(
        "ldr r0, =0x000000FF\n"
        "movs r1, #8\n"
        "rors r0, r1\n"
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "r1", "cc"
    );
    CHECK(out == 0xFF000000);
}

static void test_rsb_neg(void)
{
    int out;

    TEST("neg");
    __asm volatile(
        "movs r0, #42\n"
        "rsbs r0, r0, #0\n"  /* NEG: 0 - r0 */
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "cc"
    );
    CHECK(out == -42);

    TEST("neg_zero");
    __asm volatile(
        "movs r0, #0\n"
        "rsbs r0, r0, #0\n"
        "mov %0, r0\n"
        : "=r"(out) :: "r0", "cc"
    );
    unsigned int f = get_flags();
    CHECK(out == 0);
    CHECK(f & F_Z);
}

static void test_tst(void)
{
    unsigned int f;

    TEST("tst_zero");
    __asm volatile(
        "movs r0, #0xF0\n"
        "movs r1, #0x0F\n"
        "tst r0, r1\n"
        : : : "r0", "r1", "cc"
    );
    f = get_flags();
    CHECK(f & F_Z);  /* 0xF0 & 0x0F = 0 */

    TEST("tst_nonzero");
    __asm volatile(
        "movs r0, #0xFF\n"
        "movs r1, #0x0F\n"
        "tst r0, r1\n"
        : : : "r0", "r1", "cc"
    );
    f = get_flags();
    CHECK(!(f & F_Z));  /* 0xFF & 0x0F = 0x0F ≠ 0 */
}

static void test_rev(void)
{
    unsigned int out;

    TEST("rev");
    __asm volatile(
        "ldr r0, =0x12345678\n"
        "rev %0, r0\n"
        : "=r"(out) :: "r0"
    );
    CHECK(out == 0x78563412);
}

static void test_adr(void)
{
    unsigned int out;

    TEST("adr");
    /* ADD Rd, PC, #imm — generates address relative to PC */
    __asm volatile(
        "adr %0, 1f\n"
        "b 2f\n"
        ".align 2\n"
        "1: .word 0\n"
        "2:\n"
        : "=r"(out)
    );
    /* out should be a valid flash address */
    CHECK(out >= 0x08000000 && out < 0x08080000);
}

void test_main(void)
{
    test_adc();
    test_sbc();
    test_ror();
    test_rsb_neg();
    test_tst();
    test_rev();
    test_adr();
    TEST_DONE("alu2");
}
