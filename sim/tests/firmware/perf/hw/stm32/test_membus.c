/*
 * test_perf_membus.c — RAM read/write wall-clock throughput.
 *
 * Writes and reads 64KB of RAM, measures wall-clock time.
 *
 * Real STM32F411 at 16MHz, 0-wait-state SRAM:
 *   Write: 1 cycle/word → 16384 words / 16MHz = 1.0ms.
 *   Read:  1-2 cycles/word → ~1.5ms.
 *   With loop overhead: ~3-4ms total.
 *
 * Emulator (~55 MIPS):
 *   Write: ~6 insns/word → 98304 insns ≈ 1.8ms.
 *   Read:  ~9 insns/word → 147456 insns ≈ 2.7ms.
 *   Must complete in < 200ms total (allows for slow CI hosts).
 */
#include "test.h"

#define NUM_WORDS    (16384)  /* 64KB */
#define MAX_WALL_MS  200      /* fail if 64KB read+write takes longer */

static volatile unsigned int ram_buf[NUM_WORDS];

void test_main(void)
{
    cycle_counter_init();

    unsigned int t0 = semi_clock_us();

    TEST("ram_write_64k");
    unsigned int wc0 = cycles();
    for (int i = 0; i < NUM_WORDS; i++)
        ram_buf[i] = (unsigned int)i;
    unsigned int wc1 = cycles();

    TEST("ram_read_64k");
    unsigned int rc0 = cycles();
    volatile unsigned int sum = 0;
    for (int i = 0; i < NUM_WORDS; i++)
        sum += ram_buf[i];
    unsigned int rc1 = cycles();

    unsigned int t1 = semi_clock_us();

    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int w_per_word = (wc1 - wc0) / NUM_WORDS;
    unsigned int r_per_word = (rc1 - rc0) / NUM_WORDS;

    semi_puts("ram 64KB: ");
    semi_putdec(elapsed_ms); semi_puts("ms wall, ");
    semi_puts("w="); semi_putdec(w_per_word); semi_puts(" r=");
    semi_putdec(r_per_word); semi_puts(" cycles/word\n");

    /* Wall-clock: total read+write must be fast enough */
    CHECK(elapsed_ms < MAX_WALL_MS);
    /* Cycle sanity: 1-20 cycles per word */
    CHECK_RANGE(w_per_word, 1, 10);
    CHECK_RANGE(r_per_word, 1, 15);

    TEST_DONE("perf_membus");
}
