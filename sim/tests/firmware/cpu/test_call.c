/*
 * test_call.c — Function call tests (BL, BX LR, nested calls, stack)
 *
 * Tests that BL sets LR correctly, BX LR returns properly,
 * nested calls preserve LR on the stack, and return values work.
 */
#include "test.h"

/* Simple leaf function — no further calls */
__attribute__((noinline))
static int add_one(int x)
{
    return x + 1;
}

/* Calls another function — must save/restore LR */
__attribute__((noinline))
static int add_two(int x)
{
    return add_one(add_one(x));
}

/* 3 levels deep */
__attribute__((noinline))
static int triple_nest(int x)
{
    return add_two(x) + add_one(x);
}

/* Recursive function */
__attribute__((noinline))
static int factorial(int n)
{
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

/* Function that uses many registers to stress push/pop */
__attribute__((noinline))
static int use_many_regs(int a)
{
    int b = a + 1;
    int c = b + 2;
    int d = c + 3;
    int e = d + 4;
    int f = e + 5;
    /* Force compiler to keep all live */
    return a + b + c + d + e + f;
}

void test_main(void)
{
    TEST("bl_leaf");
    CHECK(add_one(5) == 6);
    CHECK(add_one(0) == 1);
    CHECK(add_one(-1) == 0);

    TEST("bl_nested");
    CHECK(add_two(10) == 12);
    CHECK(add_two(0) == 2);

    TEST("bl_triple");
    CHECK(triple_nest(5) == 13);  /* add_two(5)=7, add_one(5)=6, 7+6=13 */

    TEST("recursion");
    CHECK(factorial(1) == 1);
    CHECK(factorial(5) == 120);
    CHECK(factorial(10) == 3628800);

    TEST("many_regs");
    /* a=1: b=2, c=4, d=7, e=11, f=16 → sum=41 */
    CHECK(use_many_regs(1) == 41);

    TEST_DONE("call");
}
