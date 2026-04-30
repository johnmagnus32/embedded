/*
 * test_it_irq.c — IT state preservation across interrupts
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SYST_CSR   (*(volatile unsigned int *)0xE000E010)
#define SYST_RVR   (*(volatile unsigned int *)0xE000E014)
#define SYST_CVR   (*(volatile unsigned int *)0xE000E018)

static volatile int systick_fired;

void systick_handler(void)
{
    systick_fired++;
    SYST_CSR = 0;
}

void svc_handler(void) {}
void memmanage_handler(void) { while(1); }
void pendsv_handler(void) {}

static void test_it_survives_irq(void)
{
    TEST("it_across_irq");
    systick_fired = 0;
    SYST_RVR = 2;
    SYST_CVR = 0;
    SYST_CSR = 0x07;

    volatile int pass_count = 0;
    volatile int fail_count = 0;

    for (int i = 0; i < 1000; i++) {
        int r;
        __asm volatile(
            "movs r0, #1\n cmp r0, #1\n"
            "ite eq\n moveq %0, #1\n movne %0, #0\n"
            : "=r"(r) :: "r0", "cc"
        );
        if (r == 1) pass_count++;
        else fail_count++;
    }

    SYST_CSR = 0;
    CHECK(systick_fired > 0);
    CHECK(pass_count == 1000);
    CHECK(fail_count == 0);
}

extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler, 0, 0,
    memmanage_handler, 0, 0, 0, 0, 0, 0,
    svc_handler, 0, 0,
    pendsv_handler,
    systick_handler,
};

void test_main(void)
{
    test_it_survives_irq();
    TEST_DONE("it_irq");
}
