/*
 * test_mem.c — Load/store tests (byte, halfword, word, signed extends)
 *
 * Tests LDR/STR, LDRB/STRB, LDRH/STRH, LDRSB, LDRSH and
 * various addressing modes (immediate, register offset, SP-relative).
 */
#include "test.h"

static unsigned char byte_buf[16];
static unsigned short half_buf[8];
static unsigned int word_buf[8];

static void test_word_access(void)
{
    TEST("str_ldr_imm");
    volatile unsigned int *p = &word_buf[0];
    *p = 0xDEADBEEF;
    CHECK(*p == 0xDEADBEEF);

    TEST("ldr_word");
    word_buf[2] = 0x12345678;
    CHECK(word_buf[2] == 0x12345678);
}

static void test_byte_access(void)
{
    TEST("strb_ldrb");
    byte_buf[0] = 0;
    byte_buf[0] = 0xAB;
    unsigned int val;
    __asm volatile("ldrb %0, %1" : "=r"(val) : "m"(byte_buf[0]));
    CHECK(val == 0xAB);  /* zero-extended */

    TEST("ldrsb_negative");
    byte_buf[1] = 0x80;  /* -128 as signed byte */
    int sval;
    /* Use register offset form for LDRSB */
    unsigned char *p = &byte_buf[1];
    __asm volatile(
        "mov r1, #0\n"
        "ldrsb %0, [%1, r1]\n"
        : "=r"(sval) : "r"(p) : "r1"
    );
    CHECK(sval == -128);  /* sign-extended to 0xFFFFFF80 */

    TEST("ldrsb_positive");
    byte_buf[2] = 0x7F;
    p = &byte_buf[2];
    __asm volatile(
        "mov r1, #0\n"
        "ldrsb %0, [%1, r1]\n"
        : "=r"(sval) : "r"(p) : "r1"
    );
    CHECK(sval == 127);
}

static void test_half_access(void)
{
    TEST("strh_ldrh");
    half_buf[0] = 0xBEEF;
    unsigned int val;
    __asm volatile("ldrh %0, %1" : "=r"(val) : "m"(half_buf[0]));
    CHECK(val == 0xBEEF);

    TEST("ldrsh_negative");
    half_buf[1] = 0x8000;  /* -32768 as signed half */
    int sval;
    unsigned short *p = &half_buf[1];
    __asm volatile(
        "mov r1, #0\n"
        "ldrsh %0, [%1, r1]\n"
        : "=r"(sval) : "r"(p) : "r1"
    );
    CHECK(sval == -32768);
}

static void test_sp_relative(void)
{
    TEST("str_ldr_sp");
    unsigned int out;
    __asm volatile(
        "sub sp, #16\n"
        "mov r0, #0x55\n"
        "str r0, [sp, #4]\n"
        "ldr %0, [sp, #4]\n"
        "add sp, #16\n"
        : "=r"(out) :: "r0"
    );
    CHECK(out == 0x55);
}

static void test_sign_extend(void)
{
    TEST("sxtb");
    int out;
    __asm volatile("mov r0, #0x80\n sxtb %0, r0" : "=r"(out) :: "r0");
    CHECK(out == -128);

    TEST("sxth");
    __asm volatile("ldr r0, =0x8000\n sxth %0, r0" : "=r"(out) :: "r0");
    CHECK(out == -32768);

    TEST("uxtb");
    unsigned int uout;
    __asm volatile("ldr r0, =0xFFFFFF80\n uxtb %0, r0" : "=r"(uout) :: "r0");
    CHECK(uout == 0x80);

    TEST("uxth");
    __asm volatile("ldr r0, =0xFFFF8000\n uxth %0, r0" : "=r"(uout) :: "r0");
    CHECK(uout == 0x8000);
}

void test_main(void)
{
    test_word_access();
    test_byte_access();
    test_half_access();
    test_sp_relative();
    test_sign_extend();
    TEST_DONE("mem");
}
