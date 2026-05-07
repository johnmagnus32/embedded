/*
 * test_perf_audio_dma.c — DMA I2S audio throughput.
 *
 * Transfers 512 stereo samples (1024 half-words) via DMA1 Stream 4
 * to SPI2 in I2S mode. This matches the firmware's audio buffer size.
 *
 * Real STM32F411 at 16MHz, I2S at 22050Hz:
 *   512 samples * 726 cycles = 371,712 cycles = 23.2ms.
 *
 * DMA1 Stream 4 uses HISR (streams 4-7). TCIF4 = bit 5 of HISR.
 *
 * Asserts: completes in < 500ms wall-clock.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI2_CR2     (*(volatile unsigned int *)0x40003804)
#define SPI2_SR      (*(volatile unsigned int *)0x40003808)
#define SPI2_DR      (*(volatile unsigned int *)0x4000380C)
#define SPI2_I2SCFGR (*(volatile unsigned int *)0x4000381C)
#define SPI2_I2SPR   (*(volatile unsigned int *)0x40003820)

/* DMA1 Stream 4: HISR (streams 4-7) */
#define DMA1_HISR    (*(volatile unsigned int *)0x40026004)
#define DMA1_HIFCR   (*(volatile unsigned int *)0x4002600C)
/* Stream 4: base + 0x10 + 4*0x18 = 0x70 */
#define DMA1_S4CR    (*(volatile unsigned int *)0x40026070)
#define DMA1_S4NDTR  (*(volatile unsigned int *)0x40026074)
#define DMA1_S4PAR   (*(volatile unsigned int *)0x40026078)
#define DMA1_S4M0AR  (*(volatile unsigned int *)0x4002607C)

#define CR_EN      (1 << 0)
#define CR_TCIE    (1 << 4)
#define CR_DIR_M2P (1 << 6)
#define CR_MINC    (1 << 10)
#define CR_PSIZE_HALF (1 << 11)  /* 16-bit peripheral size */
#define CR_MSIZE_HALF (1 << 13)  /* 16-bit memory size */
#define DMA1_S4_TCIF (1 << 5)

#define NUM_SAMPLES 512
static short audio_buf[NUM_SAMPLES * 2];  /* L+R interleaved */

#define MAX_WALL_MS 500

void systick_handler(void) {}
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

    /* Fill audio buffer */
    for (int i = 0; i < NUM_SAMPLES * 2; i++)
        audio_buf[i] = (short)(i * 100);

    /* Enable I2S mode */
    SPI2_I2SCFGR = (1 << 10) | (1 << 11) | (2 << 8);
    SPI2_I2SPR = 0;

    TEST("audio_dma_512_samples");
    unsigned int t0 = semi_clock_us();
    unsigned int c0 = cycles();

    /* Configure DMA1 Stream 4: mem-to-periph, 16-bit, MINC, TCIE */
    DMA1_S4CR   = 0;
    DMA1_S4PAR  = (unsigned int)&SPI2_DR;
    DMA1_S4M0AR = (unsigned int)audio_buf;
    DMA1_S4NDTR = NUM_SAMPLES * 2;  /* L+R half-words */
    DMA1_S4CR   = CR_DIR_M2P | CR_MINC | CR_PSIZE_HALF | CR_MSIZE_HALF | CR_TCIE;
    DMA1_HIFCR  = DMA1_S4_TCIF;
    DMA1_S4CR  |= CR_EN;

    /* Enable I2S TXDMAEN */
    SPI2_CR2 = (1 << 1);

    /* Wait for transfer complete */
    while (!(DMA1_HISR & DMA1_S4_TCIF)) {}

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();

    SPI2_CR2 = 0;
    DMA1_HIFCR = DMA1_S4_TCIF;

    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int total_cycles = c1 - c0;
    unsigned int cycles_per_sample = total_cycles / NUM_SAMPLES;

    semi_puts("audio dma: "); semi_putdec(NUM_SAMPLES);
    semi_puts(" samples in "); semi_putdec(elapsed_ms);
    semi_puts("ms, "); semi_putdec(cycles_per_sample);
    semi_puts(" cycles/sample\n");

    CHECK(elapsed_ms < MAX_WALL_MS);
    /* Should be close to 726 cycles/sample (I2S pacing) */
    CHECK_RANGE(cycles_per_sample, 700, 800);

    TEST_DONE("perf_audio_dma");
}
