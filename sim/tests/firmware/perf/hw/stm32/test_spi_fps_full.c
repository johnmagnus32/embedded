/*
 * test_perf_fps.c — ILI9341 display frame throughput test.
 *
 * Sends 10 full 320x240 frames through the SPI → ILI9341 → chardev
 * pipeline and measures wall-clock time.
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz:
 *   320*240*2 = 153,600 bytes/frame at 8Mbps SPI = 153ms/frame ≈ 6.5 FPS.
 *   With DMA: same throughput but CPU is free during transfer.
 *
 * Emulator (unthrottled, ~55 MIPS):
 *   Polling SPI: ~10 cycles/byte → 1.5M cycles/frame ≈ 28ms/frame ≈ 35 FPS.
 *   10 frames must complete in < 5000ms (very conservative for slow CI).
 *
 * Minimum assertion: 10 frames in < 5 seconds (2 FPS floor).
 * Typical: 10 frames in ~300ms (~33 FPS).
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* SPI1 registers */
#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

/* GPIOA ODR — bit 3 is DC pin for ILI9341 */
#define GPIOA_ODR  (*(volatile unsigned int *)0x40020014)
#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define DC_PIN 3

#define WIDTH  320
#define HEIGHT 240
#define NUM_FRAMES 10
#define MAX_WALL_MS 5000

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); } /* reset DC = command */
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }        /* set DC = data */

static inline void spi_write(unsigned int byte)
{
    while (!(SPI1_SR & (1 << 1))) {}  /* wait TXE */
    SPI1_DR = byte;
}

static void ili_cmd(unsigned int cmd)
{
    dc_command();
    spi_write(cmd);
}

static void ili_set_window(int x, int y, int w, int h)
{
    /* Column address set */
    ili_cmd(0x2A);
    dc_data();
    spi_write(x >> 8); spi_write(x & 0xFF);
    spi_write((x + w - 1) >> 8); spi_write((x + w - 1) & 0xFF);

    /* Row address set */
    ili_cmd(0x2B);
    dc_data();
    spi_write(y >> 8); spi_write(y & 0xFF);
    spi_write((y + h - 1) >> 8); spi_write((y + h - 1) & 0xFF);
}

static void ili_begin_pixels(void)
{
    ili_cmd(0x2C);  /* Memory Write */
    dc_data();
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

    /* Enable SPI1: master mode */
    SPI1_CR1 = (1 << 6) | (1 << 2);  /* SPE + MSTR */

    /* Send sleep-out and display-on commands */
    ili_cmd(0x11);  /* Sleep Out */
    ili_cmd(0x29);  /* Display On */

    TEST("display_10_frames");
    unsigned int t0 = semi_clock_us();
    unsigned int c0 = cycles();

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        ili_set_window(0, 0, WIDTH, HEIGHT);
        ili_begin_pixels();

        /* Write full frame: 320*240 pixels, 2 bytes each (RGB565) */
        /* Use a simple pattern that varies per frame */
        unsigned int pixel = (frame * 0x1111) & 0xFFFF;
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            spi_write(pixel >> 8);
            spi_write(pixel & 0xFF);
        }

    }

    unsigned int t1 = semi_clock_us();
    unsigned int c1 = cycles();

    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int total_cycles = c1 - c0;
    unsigned int fps = (elapsed_ms > 0) ? (NUM_FRAMES * 1000) / elapsed_ms : 0;
    unsigned int cycles_per_frame = total_cycles / NUM_FRAMES;

    semi_puts("display: "); semi_putdec(NUM_FRAMES); semi_puts(" frames in ");
    semi_putdec(elapsed_ms); semi_puts("ms = ");
    semi_putdec(fps); semi_puts(" FPS, ");
    semi_putdec(cycles_per_frame); semi_puts(" cycles/frame\n");

    CHECK(elapsed_ms < MAX_WALL_MS);

    TEST_DONE("perf_fps");
}
