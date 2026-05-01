/*
 * test_perf_dma_fps_sustained.c — Sustained DMA partial frame FPS over 3 seconds.
 *
 * Same partial update pattern as test_perf_fps_sustained but using DMA.
 * Checks every 1-second window delivers >= 25 FPS.
 * (Lower threshold than CPU polling due to DMA per-tick overhead.)
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz with DMA:
 *   ~16,400 pixels/frame = 32,800 bytes * 16 cycles = 524,800 cycles
 *   At 16MHz: 32.8ms/frame = ~30 FPS.
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

#define DURATION_US        3000000
#define WINDOW_US          1000000
#define MIN_FPS_PER_WINDOW 25
#define MAX_WINDOWS        4
#define RECT_PIXELS        16400

static unsigned char pixel_buf[RECT_PIXELS * 2];

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }

static inline void spi_write_byte(unsigned int b)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = b;
}

static void ili_cmd(unsigned int cmd) { dc_command(); spi_write_byte(cmd); }

static void ili_set_window(void)
{
    ili_cmd(0x2A); dc_data();
    spi_write_byte(0); spi_write_byte(0);
    spi_write_byte(0); spi_write_byte(163);
    ili_cmd(0x2B); dc_data();
    spi_write_byte(0); spi_write_byte(0);
    spi_write_byte(0); spi_write_byte(99);
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
    SPI1_CR1 = (1 << 6) | (1 << 2);

    for (int i = 0; i < RECT_PIXELS * 2; i++)
        pixel_buf[i] = (unsigned char)(i & 0xFF);

    int window_fps[MAX_WINDOWS];
    int num_windows = 0;
    int total_frames = 0;
    int window_frames = 0;

    unsigned int t_start = semi_clock_us();
    unsigned int t_window = t_start;
    unsigned int t_end = t_start + DURATION_US;

    while (1) {
        unsigned int now = semi_clock_us();
        if (now >= t_end) break;

        if (now - t_window >= WINDOW_US) {
            if (num_windows < MAX_WINDOWS)
                window_fps[num_windows++] = window_frames;
            window_frames = 0;
            t_window = now;
        }

        ili_set_window();
        ili_cmd(0x2C);
        dc_data();
        dma_transfer(RECT_PIXELS * 2);

        total_frames++;
        window_frames++;
    }

    if (window_frames > 0 && num_windows < MAX_WINDOWS)
        window_fps[num_windows++] = window_frames;

    unsigned int elapsed_ms = (semi_clock_us() - t_start) / 1000;
    unsigned int avg_fps = (elapsed_ms > 0) ? (total_frames * 1000) / elapsed_ms : 0;

    semi_puts("dma sustained: "); semi_putdec(total_frames);
    semi_puts(" frames in "); semi_putdec(elapsed_ms);
    semi_puts("ms = "); semi_putdec(avg_fps); semi_puts(" FPS avg\n");

    semi_puts("per-window FPS:");
    for (int i = 0; i < num_windows; i++) {
        semi_puts(" "); semi_putdec(window_fps[i]);
    }
    semi_puts("\n");

    TEST("dma_sustained_fps");
    int check_count = (num_windows > 1) ? num_windows - 1 : num_windows;
    for (int i = 0; i < check_count; i++)
        CHECK(window_fps[i] >= MIN_FPS_PER_WINDOW);

    TEST_DONE("perf_dma_fps_sustained");
}
