/*
 * test_shift.c — Shift, bitwise, multiply, divide tests
 *
 * Tests LSL, LSR, ASR, AND, ORR, EOR, BIC, MVN, MUL, UDIV, SDIV,
 * UBFX, SBFX, REV, and CBZ/CBNZ.
 */
#include "test.h"

static void test_shifts(void)
{
    unsigned int out;

    TEST("lsl_imm");
    __asm volatile("movs r0, #1\n lsls r0, r0, #4\n mov %0, r0" : "=r"(out) :: "r0", "cc");
    CHECK(out == 16);

    TEST("lsr_imm");
    __asm volatile("mov r0, #0x80\n lsrs r0, r0, #3\n mov %0, r0" : "=r"(out) :: "r0", "cc");
    CHECK(out == 0x10);

    TEST("asr_negative");
    __asm volatile("ldr r0, =0x80000000\n asrs r0, r0, #4\n mov %0, r0" : "=r"(out) :: "r0", "cc");
    CHECK(out == 0xF8000000);  /* sign-extended */

    TEST("asr_positive");
    __asm volatile("mov r0, #0x40\n asrs r0, r0, #2\n mov %0, r0" : "=r"(out) :: "r0", "cc");
    CHECK(out == 0x10);
}

static void test_bitwise(void)
{
    unsigned int out;

    TEST("and");
    __asm volatile("mov r0, #0xFF\n mov r1, #0x0F\n ands r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 0x0F);

    TEST("orr");
    __asm volatile("mov r0, #0xF0\n mov r1, #0x0F\n orrs r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 0xFF);

    TEST("eor");
    __asm volatile("mov r0, #0xFF\n mov r1, #0x0F\n eors r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 0xF0);

    TEST("bic");
    __asm volatile("mov r0, #0xFF\n mov r1, #0x0F\n bics r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 0xF0);

    TEST("mvn");
    __asm volatile("mov r1, #0\n mvns r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 0xFFFFFFFF);
}

static void test_multiply(void)
{
    unsigned int out;

    TEST("mul");
    __asm volatile("mov r0, #7\n mov r1, #6\n muls r0, r1\n mov %0, r0" : "=r"(out) :: "r0", "r1", "cc");
    CHECK(out == 42);

    TEST("mul_wide");
    /* MUL.W rd, rn, rm (Thumb-2) */
    __asm volatile("ldr r0, =100000\n ldr r1, =200\n mul r2, r0, r1\n mov %0, r2" : "=r"(out) :: "r0", "r1", "r2");
    CHECK(out == 20000000);

    TEST("umull");
    unsigned int lo, hi;
    __asm volatile(
        "ldr r0, =0xFFFFFFFF\n"
        "ldr r1, =0x2\n"
        "umull %0, %1, r0, r1\n"
        : "=r"(lo), "=r"(hi) :: "r0", "r1"
    );
    CHECK(lo == 0xFFFFFFFE);
    CHECK(hi == 0x1);
}

static void test_divide(void)
{
    unsigned int out;

    TEST("udiv");
    __asm volatile("mov r0, #100\n mov r1, #7\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 14);

    TEST("sdiv_neg");
    int sout;
    __asm volatile("mvn r0, #99\n mov r1, #10\n sdiv %0, r0, r1" : "=r"(sout) :: "r0", "r1");
    CHECK(sout == -10);  /* -100 / 10 = -10 */

    TEST("udiv_by_zero");
    __asm volatile("mov r0, #42\n mov r1, #0\n udiv %0, r0, r1" : "=r"(out) :: "r0", "r1");
    CHECK(out == 0);  /* ARM: div by zero returns 0 */
}

static void test_bitfield(void)
{
    unsigned int out;

    TEST("ubfx");
    __asm volatile("ldr r0, =0xDEADBEEF\n ubfx %0, r0, #8, #8" : "=r"(out) :: "r0");
    CHECK(out == 0xBE);  /* bits [15:8] */

    TEST("sbfx_neg");
    int sout;
    __asm volatile("ldr r0, =0x00000080\n sbfx %0, r0, #0, #8" : "=r"(sout) :: "r0");
    CHECK(sout == -128);  /* sign-extend bit 7 */
}

static void test_cbz_cbnz(void)
{
    volatile int result;

    TEST("cbz_taken");
    result = 0;
    __asm volatile(
        "movs r0, #0\n"
        "cbz r0, 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0"
    );
    CHECK(result == 1);

    TEST("cbnz_taken");
    result = 0;
    __asm volatile(
        "movs r0, #5\n"
        "cbnz r0, 1f\n"
        "b 2f\n"
        "1: movs %0, #1\n"
        "2:\n"
        : "=r"(result) :: "r0"
    );
    CHECK(result == 1);

    TEST("cbz_not_taken");
    result = 99;
    __asm volatile(
        "movs r0, #1\n"
        "cbz r0, 1f\n"
        "b 2f\n"
        "1: movs %0, #0\n"
        "2:\n"
        : "=r"(result) :: "r0"
    );
    CHECK(result == 99);
}

void test_main(void)
{
    test_shifts();
    test_bitwise();
    test_multiply();
    test_divide();
    test_bitfield();
    test_cbz_cbnz();
    TEST_DONE("shift");
}
