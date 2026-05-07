/*
 * test_perf_dma_fps_full.c — DMA full-frame display throughput.
 *
 * Transfers 10 full 320x240 frames via DMA to SPI1.
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz:
 *   320*240*2 = 153,600 bytes/frame * 16 cycles/byte = 2,457,600 cycles
 *   At 16MHz: 153.6ms/frame = ~6.5 FPS.
 *
 * Asserts: 10 frames in < 5000ms.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_CR2 (*(volatile unsigned int *)0x40013004)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

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

#define WIDTH  320
#define HEIGHT 240
#define NUM_FRAMES 10
#define MAX_WALL_MS 5000

/* Use a small buffer and transfer in chunks (can't fit 153KB in RAM easily) */
#define CHUNK_SIZE 1024
static unsigned char chunk_buf[CHUNK_SIZE];

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }

static inline void spi_write_byte(unsigned int byte)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = byte;
}

static void ili_cmd(unsigned int cmd) { dc_command(); spi_write_byte(cmd); }

static void dma_transfer(unsigned char *buf, int nbytes)
{
    DMA2_S3CR   = 0;
    DMA2_S3PAR  = (unsigned int)&SPI1_DR;
    DMA2_S3M0AR = (unsigned int)buf;
    DMA2_S3NDTR = nbytes;
    DMA2_S3CR   = CR_DIR_M2P | CR_MINC | CR_TCIE;
    DMA2_LIFCR  = DMA2_S3_TCIF;
    DMA2_S3CR  |= CR_EN;
    SPI1_CR2 = (1 << 1);
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
    SPI1_CR1 = (1 << 6) | (1 << 2);

    for (int i = 0; i < CHUNK_SIZE; i++)
        chunk_buf[i] = (unsigned char)(i & 0xFF);

    TEST("dma_full_frame_fps");
    unsigned int t0 = semi_clock_us();
    unsigned int c0 = cycles();

    int total_bytes = WIDTH * HEIGHT * 2;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        /* Set window to full screen */
        ili_cmd(0x2A); dc_data();
        spi_write_byte(0); spi_write_byte(0);
        spi_write_byte((WIDTH-1) >> 8); spi_write_byte((WIDTH-1) & 0xFF);
        ili_cmd(0x2B); dc_data();
        spi_write_byte(0); spi_write_byte(0);
        spi_write_byte((HEIGHT-1) >> 8); spi_write_byte((HEIGHT-1) & 0xFF);
        ili_cmd(0x2C); dc_data();

        /* DMA in chunks */
        int remaining = total_bytes;
        while (remaining > 0) {
            int n = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
            dma_transfer(chunk_buf, n);
            remaining -= n;
        }
    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();
    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int fps = (elapsed_ms > 0) ? (NUM_FRAMES * 1000) / elapsed_ms : 999;
    unsigned int cycles_per_frame = (c1 - c0) / NUM_FRAMES;

    semi_puts("dma full: "); semi_putdec(NUM_FRAMES);
    semi_puts(" frames in "); semi_putdec(elapsed_ms);
    semi_puts("ms = "); semi_putdec(fps); semi_puts(" FPS, ");
    semi_putdec(cycles_per_frame); semi_puts(" cycles/frame\n");

    CHECK(elapsed_ms < MAX_WALL_MS);

    TEST_DONE("perf_dma_fps_full");
}
