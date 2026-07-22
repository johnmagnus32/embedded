/*
 * test_perf_mips.c — Wall-clock MIPS throughput test.
 *
 * Runs a fixed workload and asserts minimum instruction throughput.
 *
 * Real STM32F411 at 16MHz: 16 MIPS (most Thumb insns are 1 cycle).
 * Minimum expected emulator performance: 10 MIPS.
 * Typical emulator performance on modern x86: 40-80 MIPS.
 */
#include "test.h"

#define MIN_MIPS 50  /* fail if emulator is slower than this */

__attribute__((noinline))
static int compute(int *buf, int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        buf[i] = i * 3 + 7;
        sum += buf[i];
        if (buf[i] > 100) sum -= 10;
    }
    return sum;
}

__attribute__((noinline))
static int nested_calls(int x)
{
    int tmp[64];
    return compute(tmp, 64) + x;
}

static int workload_buf[256];

void test_main(void)
{
    cycle_counter_init();

    unsigned int c0 = cycles();
    unsigned int t0 = semi_clock_us();

    volatile int result = 0;
    for (int iter = 0; iter < 200; iter++) {
        result += compute(workload_buf, 256);
        result += nested_calls(iter);
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();

    unsigned int elapsed_us = t1 - t0;
    unsigned int insns = c1 - c0;
    unsigned int mips = (elapsed_us > 0) ? insns / elapsed_us : 0;

    semi_puts("mips: "); semi_putdec(mips);
    semi_puts(" ("); semi_putdec(insns); semi_puts(" insns in ");
    semi_putdec(elapsed_us); semi_puts(" us)\n");

    TEST("mips_throughput");
    CHECK(mips >= MIN_MIPS);

    if (result == 0x7FFFFFFF) semi_puts("unlikely\n");
    TEST_DONE("perf_mips");
}
