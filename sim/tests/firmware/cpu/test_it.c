/*
 * test_it.c — IT (If-Then) block tests
 *
 * Tests conditional execution inside IT blocks, including
 * multi-instruction IT blocks with then/else lanes.
 */
#include "test.h"

static void test_it_basic(void)
{
    volatile int result;

    TEST("it_then_exec");
    result = 0;
    __asm volatile(
        "movs r0, #5\n"
        "cmp r0, #5\n"
        "it eq\n"
        "moveq %0, #1\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 1);

    TEST("it_then_skip");
    result = 99;
    __asm volatile(
        "movs r0, #5\n"
        "cmp r0, #10\n"
        "it eq\n"
        "moveq %0, #0\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 99);  /* moveq should NOT execute */
}

static void test_ite(void)
{
    volatile int result;

    TEST("ite_then");
    result = 0;
    __asm volatile(
        "movs r0, #1\n"
        "cmp r0, #1\n"
        "ite eq\n"
        "moveq %0, #10\n"
        "movne %0, #20\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 10);

    TEST("ite_else");
    result = 0;
    __asm volatile(
        "movs r0, #1\n"
        "cmp r0, #2\n"
        "ite eq\n"
        "moveq %0, #10\n"
        "movne %0, #20\n"
        : "=r"(result) :: "r0", "cc"
    );
    CHECK(result == 20);
}

static void test_itt(void)
{
    volatile int r1, r2;

    TEST("itt_both_exec");
    r1 = 0; r2 = 0;
    __asm volatile(
        "movs r0, #3\n"
        "cmp r0, #3\n"
        "itt eq\n"
        "moveq %0, #1\n"
        "moveq %1, #2\n"
        : "=r"(r1), "=r"(r2) :: "r0", "cc"
    );
    CHECK(r1 == 1);
    CHECK(r2 == 2);

    TEST("itt_both_skip");
    r1 = 99; r2 = 99;
    __asm volatile(
        "movs r0, #3\n"
        "cmp r0, #4\n"
        "itt eq\n"
        "moveq %0, #1\n"
        "moveq %1, #2\n"
        : "+r"(r1), "+r"(r2) :: "r0", "cc"
    );
    CHECK(r1 == 99);
    CHECK(r2 == 99);
}

void test_main(void)
{
    test_it_basic();
    test_ite();
    test_itt();
    TEST_DONE("it_block");
}
