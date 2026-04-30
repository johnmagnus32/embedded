/*
 * test_irq.c — Interrupt and exception tests
 *
 * Tests SysTick firing, exception frame push/pop, EXC_RETURN,
 * PSP/MSP switching, PRIMASK, and IT state preservation across interrupts.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* System registers */
#define SYST_CSR   (*(volatile unsigned int *)0xE000E010)
#define SYST_RVR   (*(volatile unsigned int *)0xE000E014)
#define SYST_CVR   (*(volatile unsigned int *)0xE000E018)
#define SCB_ICSR   (*(volatile unsigned int *)0xE000ED04)
#define SCB_SHPR3  (*(volatile unsigned int *)0xE000ED20)

#define ICSR_PENDSVSET (1 << 28)

static volatile int systick_count;
static volatile int pendsv_count;
static volatile int pendsv_saw_psp;
static volatile unsigned int pendsv_saved_lr;

void systick_handler(void)
{
    systick_count++;
    SYST_CSR = 0;
}

void pendsv_handler(void)
{
    pendsv_count++;
    unsigned int lr;
    __asm volatile("mov %0, lr" : "=r"(lr));
    pendsv_saved_lr = lr;
    pendsv_saw_psp = (lr & 0x4) ? 1 : 0;
}

void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

static void test_systick(void)
{
    TEST("systick_fires");
    systick_count = 0;
    SYST_RVR = 100;
    SYST_CVR = 0;
    SYST_CSR = 0x07;
    for (volatile int i = 0; i < 100000 && systick_count == 0; i++) {}
    CHECK(systick_count == 1);
}

static void test_pendsv(void)
{
    TEST("pendsv_fires");
    pendsv_count = 0;
    SCB_SHPR3 |= (0xFF << 16);
    SCB_ICSR = ICSR_PENDSVSET;
    for (volatile int i = 0; i < 1000 && pendsv_count == 0; i++) {}
    CHECK(pendsv_count == 1);
}

static void test_exc_return_msp(void)
{
    TEST("exc_return_msp");
    pendsv_count = 0;
    pendsv_saved_lr = 0;
    SCB_ICSR = ICSR_PENDSVSET;
    for (volatile int i = 0; i < 1000 && pendsv_count == 0; i++) {}
    CHECK(pendsv_saved_lr == 0xFFFFFFF9);
    CHECK(pendsv_saw_psp == 0);
}

static void test_psp_switch(void)
{
    TEST("psp_switch");
    static unsigned int psp_stack[256];
    unsigned int psp_top = (unsigned int)&psp_stack[256];
    pendsv_count = 0;
    pendsv_saved_lr = 0;
    __asm volatile(
        "msr psp, %0\n mov r0, #2\n msr control, r0\n isb\n"
        : : "r"(psp_top) : "r0"
    );
    SCB_ICSR = ICSR_PENDSVSET;
    for (volatile int i = 0; i < 1000 && pendsv_count == 0; i++) {}
    CHECK(pendsv_saved_lr == 0xFFFFFFFD);
    CHECK(pendsv_saw_psp == 1);
    __asm volatile("mov r0, #0\n msr control, r0\n isb" ::: "r0");
}

static void test_primask(void)
{
    TEST("primask_blocks");
    volatile int masked_count;
    __asm volatile(
        "cpsid i\n"
        "ldr r0, =0xE000E010\n"
        "movs r1, #0\n"
        "str r1, [r0, #0]\n"
        "str r1, %[sc]\n"
        "movs r1, #100\n"
        "str r1, [r0, #4]\n"
        "movs r1, #0\n"
        "str r1, [r0, #8]\n"
        "movs r1, #7\n"
        "str r1, [r0, #0]\n"
        "movs r2, #0\n"
        "1: adds r2, #1\n"
        "cmp r2, #250\n"
        "blt 1b\n"
        "movs r1, #0\n"
        "str r1, [r0, #0]\n"
        "ldr %[mc], %[sc]\n"
        : [mc] "=r"(masked_count)
        : [sc] "m"(systick_count)
        : "r0", "r1", "r2", "cc", "memory"
    );
    CHECK(masked_count == 0);
    __asm volatile("cpsie i\n nop\n nop\n nop\n nop" ::: "memory");
    CHECK(systick_count >= 1);
}

static void test_exception_preserves_regs(void)
{
    TEST("exc_preserves_regs");
    unsigned int r0_out, r1_out, r2_out, r3_out, r12_out;
    pendsv_count = 0;
    __asm volatile(
        "mov r0, #0xA0\n mov r1, #0xB1\n mov r2, #0xC2\n"
        "mov r3, #0xD3\n mov r12, #0xE4\n"
        "ldr r4, =0xE000ED04\n ldr r5, =0x10000000\n str r5, [r4]\n"
        "1: ldr r4, %5\n cmp r4, #0\n beq 1b\n"
        "mov %0, r0\n mov %1, r1\n mov %2, r2\n mov %3, r3\n mov %4, r12\n"
        : "=r"(r0_out), "=r"(r1_out), "=r"(r2_out), "=r"(r3_out), "=r"(r12_out)
        : "m"(pendsv_count)
        : "r0", "r1", "r2", "r3", "r4", "r5", "r12", "cc", "memory"
    );
    CHECK(r0_out == 0xA0);
    CHECK(r1_out == 0xB1);
    CHECK(r2_out == 0xC2);
    CHECK(r3_out == 0xD3);
    CHECK(r12_out == 0xE4);
}

/* Custom vector table with ISR entries */
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
    test_systick();
    test_pendsv();
    test_exc_return_msp();
    test_psp_switch();
    test_primask();
    test_exception_preserves_regs();
    TEST_DONE("irq");
}
