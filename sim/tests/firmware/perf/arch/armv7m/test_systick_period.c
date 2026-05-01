/*
 * test_perf_systick.c — SysTick wall-clock timing test.
 *
 * Configures SysTick at 1kHz, counts 100 ticks, measures wall-clock time.
 *
 * Real STM32F411 at 16MHz with RVR=15999 (1kHz):
 *   100 ticks = exactly 100ms wall-clock.
 *
 * Emulator (unthrottled, ~55 MIPS):
 *   100 ticks = 100 * 16000 insns = 1.6M insns ≈ 29ms wall-clock.
 *   Must complete in < 500ms (allows for slow CI hosts).
 *
 * Also verifies cycle-count accuracy: 100 ticks should be ~1,600,000 cycles.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SYST_CSR (*(volatile unsigned int *)0xE000E010)
#define SYST_RVR (*(volatile unsigned int *)0xE000E014)
#define SYST_CVR (*(volatile unsigned int *)0xE000E018)

#define TICK_HZ       1000
#define SYSCLK_HZ     16000000
#define RVR_VALUE     ((SYSCLK_HZ / TICK_HZ) - 1)  /* 15999 */
#define NUM_TICKS     100
#define MAX_WALL_MS   500   /* fail if 100 ticks take longer than this */

static volatile int tick_count;

void systick_handler(void) { tick_count++; }
void pendsv_handler(void) {}
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

    TEST("systick_100_ticks_wallclock");
    tick_count = 0;
    SYST_RVR = RVR_VALUE;
    SYST_CVR = 0;

    unsigned int c0 = cycles();
    unsigned int t0 = semi_clock_us();
    SYST_CSR = 0x07;

    while (tick_count < NUM_TICKS) {}

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();
    SYST_CSR = 0;

    unsigned int elapsed_us = t1 - t0;
    unsigned int elapsed_ms = elapsed_us / 1000;
    unsigned int elapsed_cycles = c1 - c0;

    semi_puts("systick 100 ticks: ");
    semi_putdec(elapsed_ms); semi_puts("ms wall, ");
    semi_putdec(elapsed_cycles); semi_puts(" cycles\n");

    /* Wall-clock: must complete in reasonable time */
    CHECK(elapsed_ms < MAX_WALL_MS);

    /* Cycle accuracy: 100 ticks * 16000 cycles/tick = 1,600,000 ± 5% */
    unsigned int expected_cycles = NUM_TICKS * (RVR_VALUE + 1);
    CHECK_RANGE(elapsed_cycles, expected_cycles - expected_cycles/20,
                                expected_cycles + expected_cycles/20);

    TEST_DONE("perf_systick");
}
