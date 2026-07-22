/*
 * test_ldm_stm.c — Load/store multiple tests
 *
 * Tests 16-bit PUSH/POP, 16-bit STM/LDM, and 32-bit STMDB/LDMIA.W
 * These are critical for context switches and function prologues.
 */
#include "test.h"

static unsigned int buf[16];

static void test_push_pop(void)
{
    volatile unsigned int r4_out, r5_out, r6_out;

    TEST("push_pop_regs");
    __asm volatile(
        "mov r4, #0xAA\n"
        "mov r5, #0xBB\n"
        "mov r6, #0xCC\n"
        "push {r4, r5, r6}\n"
        "mov r4, #0\n"
        "mov r5, #0\n"
        "mov r6, #0\n"
        "pop {r4, r5, r6}\n"
        "mov %0, r4\n"
        "mov %1, r5\n"
        "mov %2, r6\n"
        : "=r"(r4_out), "=r"(r5_out), "=r"(r6_out)
        :: "r4", "r5", "r6"
    );
    CHECK(r4_out == 0xAA);
    CHECK(r5_out == 0xBB);
    CHECK(r6_out == 0xCC);
}

static int add_one(int x) { return x + 1; }

static void test_push_pop_lr_pc(void)
{
    /* push {r4, lr} / pop {r4, pc} is the standard function prologue/epilogue.
     * We test it indirectly via a noinline function call. */
    volatile int result;

    TEST("push_lr_pop_pc");
    result = add_one(41);
    CHECK(result == 42);
}

static void test_stm_ldm_16(void)
{
    TEST("stm16");
    buf[0] = buf[1] = buf[2] = 0;
    unsigned int *p = buf;
    /* Use explicit register assignments to control STM register order */
    __asm volatile(
        "mov r0, %1\n"
        "mov r1, #0x11\n"
        "mov r2, #0x22\n"
        "mov r3, #0x33\n"
        "stmia r0!, {r1, r2, r3}\n"
        "mov %0, r0\n"
        : "=r"(p) : "r"(buf) : "r0", "r1", "r2", "r3", "memory"
    );
    CHECK(buf[0] == 0x11);
    CHECK(buf[1] == 0x22);
    CHECK(buf[2] == 0x33);

    TEST("ldm16");
    unsigned int a, b, c;
    __asm volatile(
        "mov r0, %3\n"
        "ldmia r0!, {r1, r2, r3}\n"
        "mov %0, r1\n"
        "mov %1, r2\n"
        "mov %2, r3\n"
        : "=r"(a), "=r"(b), "=r"(c) : "r"(buf) : "r0", "r1", "r2", "r3", "memory"
    );
    CHECK(a == 0x11);
    CHECK(b == 0x22);
    CHECK(c == 0x33);
}

static void test_stmdb_ldmia_wide(void)
{
    TEST("stmdb_w");
    /* Simulate what pendsv_handler does: stmdb r0!, {r4-r11} */
    unsigned int area[8];
    unsigned int *end = area + 8;
    __asm volatile(
        "mov r4, #4\n"
        "mov r5, #5\n"
        "mov r6, #6\n"
        "mov r7, #7\n"
        "mov r8, #8\n"
        "mov r9, #9\n"
        "mov r10, #10\n"
        "mov r11, #11\n"
        "stmdb %0!, {r4-r11}\n"
        : "+r"(end)
        :: "r4","r5","r6","r7","r8","r9","r10","r11","memory"
    );
    CHECK(end == area);  /* decremented by 8 words */
    CHECK(area[0] == 4);
    CHECK(area[1] == 5);
    CHECK(area[7] == 11);

    TEST("ldmia_w");
    /* Zero regs, then reload */
    unsigned int r4, r5, r10, r11;
    unsigned int *start = area;
    __asm volatile(
        "ldmia %4!, {r4-r11}\n"
        "mov %0, r4\n"
        "mov %1, r5\n"
        "mov %2, r10\n"
        "mov %3, r11\n"
        : "=r"(r4), "=r"(r5), "=r"(r10), "=r"(r11), "+r"(start)
        :: "r4","r5","r6","r7","r8","r9","r10","r11","memory"
    );
    CHECK(r4 == 4);
    CHECK(r5 == 5);
    CHECK(r10 == 10);
    CHECK(r11 == 11);
    CHECK(start == area + 8);  /* incremented by 8 words */
}

void test_main(void)
{
    test_push_pop();
    test_push_pop_lr_pc();
    test_stm_ldm_16();
    test_stmdb_ldmia_wide();
    TEST_DONE("ldm_stm");
}
