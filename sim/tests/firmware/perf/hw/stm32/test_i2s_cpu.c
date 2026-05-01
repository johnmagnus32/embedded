/*
 * test_perf_audio_cpu.c — CPU polling I2S audio throughput.
 *
 * Writes 1000 stereo samples to SPI2 in I2S mode via CPU polling.
 * Measures cycles and wall-clock time.
 *
 * Real STM32F411 at 16MHz, I2S at 22050Hz:
 *   726 cycles per stereo sample (2 x 16-bit words).
 *   1000 samples = 726,000 cycles = 45.4ms.
 *
 * Asserts: completes in < 500ms wall-clock.
 */
#include "test.h"

/* SPI2 / I2S2 at 0x40003800 */
#define SPI2_CR1     (*(volatile unsigned int *)0x40003800)
#define SPI2_SR      (*(volatile unsigned int *)0x40003808)
#define SPI2_DR      (*(volatile unsigned int *)0x4000380C)
#define SPI2_I2SCFGR (*(volatile unsigned int *)0x4000381C)
#define SPI2_I2SPR   (*(volatile unsigned int *)0x40003820)

#define NUM_SAMPLES  1000
#define MAX_WALL_MS  500

void test_main(void)
{
    cycle_counter_init();

    /* Enable I2S mode: I2SE=1, I2SMOD=1, I2SCFG=10 (master TX) */
    SPI2_I2SCFGR = (1 << 10) | (1 << 11) | (2 << 8);
    /* Set prescaler: default gives 726 cycles/sample */
    SPI2_I2SPR = 0;

    TEST("audio_cpu_1000_samples");
    unsigned int t0 = semi_clock_us();
    unsigned int c0 = cycles();

    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* Left channel */
        while (!(SPI2_SR & (1 << 1))) {}
        SPI2_DR = (unsigned int)(i & 0x7FFF);
        /* Right channel */
        while (!(SPI2_SR & (1 << 1))) {}
        SPI2_DR = (unsigned int)(i & 0x7FFF);
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();
    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int total_cycles = c1 - c0;
    unsigned int cycles_per_sample = total_cycles / NUM_SAMPLES;

    semi_puts("audio cpu: "); semi_putdec(NUM_SAMPLES);
    semi_puts(" samples in "); semi_putdec(elapsed_ms);
    semi_puts("ms, "); semi_putdec(cycles_per_sample);
    semi_puts(" cycles/sample\n");

    CHECK(elapsed_ms < MAX_WALL_MS);
    /* Should be close to 726 cycles/sample (I2S pacing) */
    CHECK_RANGE(cycles_per_sample, 700, 800);

    TEST_DONE("perf_audio_cpu");
}
