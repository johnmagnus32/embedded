/*
 * test_perf_dma_fps.c — DMA-driven partial display frame throughput.
 *
 * Uses DMA2 Stream 3 to transfer pixel data to SPI1 (ILI9341).
 * Commands sent via CPU polling, pixel data via DMA.
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz with DMA:
 *   16,400 pixels * 2 bytes * 16 cycles = 524,800 cycles/frame.
 *   At 16MHz: 32.8ms/frame = ~30 FPS. CPU is free during transfer.
 *
 * Asserts: 10 frames in < 2000ms.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_CR2 (*(volatile unsigned int *)0x40013004)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

/* DMA2 Stream 3 — uses LISR (streams 0-3), not HISR (streams 4-7) */
#define DMA2_LISR    (*(volatile unsigned int *)0x40026400)
#define DMA2_LIFCR   (*(volatile unsigned int *)0x40026408)
#define DMA2_S3CR    (*(volatile unsigned int *)0x40026458)
#define DMA2_S3NDTR  (*(volatile unsigned int *)0x4002645C)
#define DMA2_S3PAR   (*(volatile unsigned int *)0x40026460)
#define DMA2_S3M0AR  (*(volatile unsigned int *)0x40026464)

#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define DC_PIN 3

#define CR_EN      (1 << 0)
#define CR_TCIE    (1 << 4)
#define CR_DIR_M2P (1 << 6)
#define CR_MINC    (1 << 10)
#define DMA2_S3_TCIF (1 << 27)

#define NUM_FRAMES   10
#define RECT_PIXELS  16400
#define MAX_WALL_MS  2000

static unsigned char pixel_buf[RECT_PIXELS * 2];

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }

static inline void spi_write_byte(unsigned int byte)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = byte;
}

static void ili_cmd(unsigned int cmd) { dc_command(); spi_write_byte(cmd); }

static void ili_set_window(int x, int y, int w, int h)
{
    ili_cmd(0x2A); dc_data();
    spi_write_byte(x >> 8); spi_write_byte(x & 0xFF);
    spi_write_byte((x + w - 1) >> 8); spi_write_byte((x + w - 1) & 0xFF);
    ili_cmd(0x2B); dc_data();
    spi_write_byte(y >> 8); spi_write_byte(y & 0xFF);
    spi_write_byte((y + h - 1) >> 8); spi_write_byte((y + h - 1) & 0xFF);
}

static void dma_transfer(int nbytes)
{
    DMA2_S3CR   = 0;
    DMA2_S3PAR  = (unsigned int)&SPI1_DR;
    DMA2_S3M0AR = (unsigned int)pixel_buf;
    DMA2_S3NDTR = nbytes;
    DMA2_S3CR   = CR_DIR_M2P | CR_MINC | CR_TCIE;
    DMA2_LIFCR  = DMA2_S3_TCIF;
    DMA2_S3CR  |= CR_EN;
    SPI1_CR2 = (1 << 1);  /* TXDMAEN */
    while (!(DMA2_LISR & DMA2_S3_TCIF)) {}
    SPI1_CR2 = 0;
    DMA2_LIFCR = DMA2_S3_TCIF;
}

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
    SPI1_CR1 = (1 << 6) | (1 << 2);  /* SPE + MSTR, BR=000 (/2) */

    for (int i = 0; i < RECT_PIXELS * 2; i++)
        pixel_buf[i] = (unsigned char)(i & 0xFF);

    TEST("dma_partial_fps");
    unsigned int t0 = semi_clock_us();
    unsigned int c0 = cycles();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        ili_set_window(0, 0, 164, 100);
        ili_cmd(0x2C);
        dc_data();
        dma_transfer(RECT_PIXELS * 2);
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();
    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int fps = (elapsed_ms > 0) ? (NUM_FRAMES * 1000) / elapsed_ms : 999;
    unsigned int cycles_per_frame = (c1 - c0) / NUM_FRAMES;

    semi_puts("dma display: "); semi_putdec(NUM_FRAMES);
    semi_puts(" frames in "); semi_putdec(elapsed_ms);
    semi_puts("ms = "); semi_putdec(fps); semi_puts(" FPS, ");
    semi_putdec(cycles_per_frame); semi_puts(" cycles/frame\n");

    CHECK(elapsed_ms < MAX_WALL_MS);

    TEST_DONE("perf_dma_fps");
}
