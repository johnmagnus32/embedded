/*
 * test_perf_fps_sustained.c — Sustained FPS consistency over 3 seconds.
 *
 * Runs partial frame updates for 3 wall-clock seconds, checking that
 * every 1-second window delivers at least 30 frames. Catches both
 * throughput issues (too slow overall) and latency spikes (one window
 * drops below 30 while others are fine).
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz:
 *   ~16,400 pixels/frame = 32,800 bytes/frame.
 *   At 8Mbps: 32.8ms/frame = ~30 FPS (right at the limit).
 *
 * Emulator: should sustain >30 FPS easily since SPI is instant.
 * Asserts: every 1-second window has >= 30 frames.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define DC_PIN 3

#define DURATION_US   3000000  /* run for 3 seconds */
#define WINDOW_US     1000000  /* check every 1 second */
#define MIN_FPS_PER_WINDOW 50
#define MAX_WINDOWS   4        /* 3 seconds + 1 spare */

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }

static inline void spi_write(unsigned int byte)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = byte;
}

static void ili_cmd(unsigned int cmd)
{
    dc_command();
    spi_write(cmd);
}

static void draw_rect(int x, int y, int w, int h, unsigned int color)
{
    ili_cmd(0x2A);
    dc_data();
    spi_write(x >> 8); spi_write(x & 0xFF);
    spi_write((x + w - 1) >> 8); spi_write((x + w - 1) & 0xFF);

    ili_cmd(0x2B);
    dc_data();
    spi_write(y >> 8); spi_write(y & 0xFF);
    spi_write((y + h - 1) >> 8); spi_write((y + h - 1) & 0xFF);

    ili_cmd(0x2C);
    dc_data();
    int npix = w * h;
    unsigned int hi = color >> 8, lo = color & 0xFF;
    for (int i = 0; i < npix; i++) {
        spi_write(hi);
        spi_write(lo);
    }
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
    ili_cmd(0x11);
    ili_cmd(0x29);

    int window_fps[MAX_WINDOWS];
    int num_windows = 0;
    int total_frames = 0;
    int window_frames = 0;

    unsigned int t_start = semi_clock_us();
    unsigned int t_window = t_start;
    unsigned int t_end = t_start + DURATION_US;
    int frame = 0;

    while (1) {
        unsigned int now = semi_clock_us();
        if (now >= t_end) break;

        /* Check if current window is complete */
        if (now - t_window >= WINDOW_US) {
            if (num_windows < MAX_WINDOWS)
                window_fps[num_windows++] = window_frames;
            window_frames = 0;
            t_window = now;
        }

        /* Render one game-like frame (~16,400 pixels) */
        int px = 40 + (frame & 7);
        draw_rect(px - 1, 180, 16, 20, 0x0000);
        draw_rect(px, 180, 16, 20, 0xFFE0);
        for (int o = 0; o < 3; o++) {
            int ox = 100 + o * 80 - (frame % 60);
            draw_rect(ox + 1, 170, 12, 30, 0x0000);
            draw_rect(ox, 170, 12, 30, 0xF800);
        }
        draw_rect(0, 0, 320, 40, 0x001F);
        draw_rect(250, 5, 60, 14, 0xFFFF);

        frame++;
        window_frames++;
        total_frames++;
    }

    /* Capture final partial window */
    if (window_frames > 0 && num_windows < MAX_WINDOWS)
        window_fps[num_windows++] = window_frames;

    unsigned int elapsed_ms = (semi_clock_us() - t_start) / 1000;
    unsigned int avg_fps = (elapsed_ms > 0) ? (total_frames * 1000) / elapsed_ms : 0;

    semi_puts("sustained: "); semi_putdec(total_frames);
    semi_puts(" frames in "); semi_putdec(elapsed_ms); semi_puts("ms = ");
    semi_putdec(avg_fps); semi_puts(" FPS avg\n");

    semi_puts("per-window FPS:");
    for (int i = 0; i < num_windows; i++) {
        semi_puts(" "); semi_putdec(window_fps[i]);
    }
    semi_puts("\n");

    /* Assert every full 1-second window hit 30 FPS */
    TEST("sustained_fps_consistency");
    int min_window = 999;
    /* Check all windows except the last (which may be partial) */
    int check_count = (num_windows > 1) ? num_windows - 1 : num_windows;
    for (int i = 0; i < check_count; i++) {
        if (window_fps[i] < min_window) min_window = window_fps[i];
        if (window_fps[i] < MIN_FPS_PER_WINDOW) {
            semi_puts("FAIL: window "); semi_putdec(i);
            semi_puts(" only "); semi_putdec(window_fps[i]); semi_puts(" FPS\n");
        }
        CHECK(window_fps[i] >= MIN_FPS_PER_WINDOW);
    }

    semi_puts("min window: "); semi_putdec(min_window); semi_puts(" FPS\n");

    TEST_DONE("perf_fps_sustained");
}
