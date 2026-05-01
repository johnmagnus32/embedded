/*
 * test_perf_partial_fps.c — Partial frame update throughput at 30 FPS.
 *
 * Simulates a game rendering loop that updates ~21.7% of a 320x240
 * display per frame via SPI, then triggers vsync.
 *
 * Real STM32F411 at 16MHz, SPI1 at 8MHz (APB2/2):
 *   Budget per frame at 30 FPS: 33,333 bytes (8Mbps / 30).
 *   21.7% of 320x240 = 16,666 pixels = 33,332 bytes.
 *   So 30 FPS is the theoretical max for this update size at 8MHz SPI.
 *   With DMA and CPU overlap, achievable in practice.
 *
 * Emulator (unthrottled, no SPI pacing):
 *   SPI writes are instant, so this should be much faster than 30 FPS.
 *   Asserts: 30 frames in < 1000ms (i.e., at least 30 FPS).
 *   Typical: ~200+ FPS on a modern host.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define DC_PIN 3

#define WIDTH  320
#define HEIGHT 240
#define PIXELS_PER_FRAME 16666  /* ~21.7% of 320*240 */
#define BYTES_PER_FRAME  (PIXELS_PER_FRAME * 2)  /* RGB565 */
#define NUM_FRAMES 30
#define MIN_FPS    30
#define MAX_WALL_MS 1000  /* 30 frames in < 1s = at least 30 FPS */

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

/* Simulate a partial screen update: set a window and fill it */
static void draw_rect(int x, int y, int w, int h, unsigned int color)
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

    /* Memory write */
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
    cycle_counter_init();
    SPI1_CR1 = (1 << 6) | (1 << 2);  /* SPE + MSTR */

    /* Init display */
    ili_cmd(0x11);  /* Sleep Out */
    ili_cmd(0x29);  /* Display On */

    TEST("partial_30fps");
    unsigned int t0 = semi_clock_us();

    /*
     * Simulate a game frame: multiple draw_rect calls totaling
     * ~16,666 pixels per frame (matching 8MHz SPI budget at 30 FPS).
     *
     * Layout per frame:
     *   - Clear player old pos:  16x20 = 320 px
     *   - Draw player new pos:   16x20 = 320 px
     *   - Clear 3 obstacles:     12x30 * 3 = 1080 px
     *   - Draw 3 obstacles:      12x30 * 3 = 1080 px
     *   - Background strip:      320x40 = 12800 px
     *   - Score area:             60x14 = 840 px
     *   Total: ~16,440 pixels ≈ 21.4% of screen
     */
    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        int px = 40 + (frame & 3);  /* slight movement */

        /* Clear + draw player */
        draw_rect(px - 1, 180, 16, 20, 0x0000);  /* clear old */
        draw_rect(px, 180, 16, 20, 0xFFE0);      /* draw new (yellow) */

        /* Clear + draw 3 obstacles */
        for (int o = 0; o < 3; o++) {
            int ox = 100 + o * 80 - frame;
            draw_rect(ox + 1, 170, 12, 30, 0x0000);  /* clear */
            draw_rect(ox, 170, 12, 30, 0xF800);       /* draw (red) */
        }

        /* Background strip (sky redraw) */
        draw_rect(0, 0, 320, 40, 0x001F);  /* blue sky strip */

        /* Score */
        draw_rect(250, 5, 60, 14, 0xFFFF);  /* white score bg */

    }

    unsigned int t1 = semi_clock_us();
    unsigned int elapsed_ms = (t1 - t0) / 1000;
    unsigned int fps = (elapsed_ms > 0) ? (NUM_FRAMES * 1000) / elapsed_ms : 999;

    semi_puts("partial update: "); semi_putdec(NUM_FRAMES);
    semi_puts(" frames in "); semi_putdec(elapsed_ms);
    semi_puts("ms = "); semi_putdec(fps); semi_puts(" FPS\n");

    CHECK(elapsed_ms < MAX_WALL_MS);

    TEST_DONE("perf_partial_fps");
}
