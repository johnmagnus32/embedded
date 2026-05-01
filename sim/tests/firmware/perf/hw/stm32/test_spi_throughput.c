/*
 * test_perf_spi.c — SPI transfer wall-clock throughput.
 *
 * Transfers 4KB via SPI polling, measures wall-clock time.
 *
 * Real STM32F411 SPI1 at max speed (APB2/2 = 8MHz SPI clock):
 *   4096 bytes * 8 bits / 8MHz = 4.1ms wall-clock.
 *   With polling overhead: ~6-8ms.
 *
 * Emulator (no SPI pacing, ~55 MIPS):
 *   ~9 cycles/byte * 4096 = 36864 insns ≈ 0.7ms wall-clock.
 *   Must complete in < 100ms (allows for slow CI hosts).
 */
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

#define NUM_BYTES    4096
#define MAX_WALL_MS  100  /* fail if 4KB SPI transfer takes longer */

void test_main(void)
{
    cycle_counter_init();
    SPI1_CR1 = (1 << 6) | (1 << 2); /* SPE + MSTR */

    TEST("spi_4k_wallclock");
    unsigned int c0 = cycles();
    unsigned int t0 = semi_clock_us();

    for (int i = 0; i < NUM_BYTES; i++) {
        while (!(SPI1_SR & (1 << 1))) {} /* wait TXE */
        SPI1_DR = (unsigned int)(i & 0xFF);
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();

    unsigned int elapsed_us = t1 - t0;
    unsigned int elapsed_ms = elapsed_us / 1000;
    unsigned int total_cycles = c1 - c0;
    unsigned int cycles_per_byte = total_cycles / NUM_BYTES;

    semi_puts("spi 4KB: ");
    semi_putdec(elapsed_ms); semi_puts("ms wall, ");
    semi_putdec(cycles_per_byte); semi_puts(" cycles/byte\n");

    CHECK(elapsed_ms < MAX_WALL_MS);
    /* Sanity: at least 1 cycle per byte, no more than 100 */
    CHECK(cycles_per_byte >= 1);
    CHECK(cycles_per_byte < 100);

    TEST_DONE("perf_spi");
}
