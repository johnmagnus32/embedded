/*
 * test_perf_irq_latency.c — Interrupt entry latency (cycles + wall-clock).
 *
 * Triggers PendSV 1000 times, measures total wall-clock time and
 * average cycle latency per interrupt.
 *
 * Real Cortex-M4: 12 cycles entry latency per interrupt.
 *   1000 interrupts at 16MHz ≈ 0.75ms (12 cycles * 1000 / 16MHz).
 *
 * Emulator: ~2-4 cycles entry latency.
 *   1000 interrupts should complete in < 50ms wall-clock.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SCB_ICSR  (*(volatile unsigned int *)0xE000ED04)
#define SCB_SHPR3 (*(volatile unsigned int *)0xE000ED20)

#define NUM_IRQS     1000
#define MAX_WALL_MS  50   /* fail if 1000 interrupts take longer than this */

static volatile int irq_count;

void pendsv_handler(void) { irq_count++; }
void systick_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

extern unsigned int _stack_top;
void test_main(void);
void __attribute__((naked)) reset_handler(void) { __asm volatile("bl test_main\n b .\n"); }

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top, reset_handler,
    0, 0, memmanage_handler, 0, 0, 0, 0, 0, 0,
    svc_handler, 0, 0, pendsv_handler, systick_handler,
};

void test_main(void)
{
    cycle_counter_init();
    SCB_SHPR3 |= (0xFF << 16);

    TEST("irq_latency_1000");
    irq_count = 0;

    unsigned int c0 = cycles();
    unsigned int t0 = semi_clock_us();

    for (int i = 0; i < NUM_IRQS; i++) {
        SCB_ICSR = (1 << 28);  /* PENDSVSET */
        while (irq_count <= i) {}
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();

    unsigned int elapsed_us = t1 - t0;
    unsigned int elapsed_ms = elapsed_us / 1000;
    unsigned int total_cycles = c1 - c0;
    unsigned int avg_cycles = total_cycles / NUM_IRQS;

    semi_puts("irq 1000x: ");
    semi_putdec(elapsed_ms); semi_puts("ms wall, ");
    semi_putdec(avg_cycles); semi_puts(" cycles/irq avg\n");

    CHECK(elapsed_ms < MAX_WALL_MS);
    /* Average latency: real HW is 12 cycles, emulator should be < 50 */
    CHECK(avg_cycles < 50);

    TEST_DONE("perf_irq_latency");
}
