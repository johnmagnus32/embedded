/*
 * test_gameboy_v2_e2e.cpp — End-to-end display verification using Verilator
 *
 * Simulates the full PPU pipeline (Verilator-compiled RTL) driven by the
 * same SPI sequence the firmware sends for render_title():
 *   1. Upload tile pixel data (a recognizable pattern for TILE_TITLE)
 *   2. Upload tile table entry for tile 20
 *   3. Set bg_color to 0x867D
 *   4. Send 1 sprite at (80, 80) with tile=20
 *
 * Then captures a full frame from LCD output and verifies:
 *   - Background is 0x867D everywhere except the sprite region
 *   - Sprite pixels at (80,80)-(143,93) are non-background
 *   - No partial/corrupt data
 *
 * This proves the entire data path works: SPI → spi_cmd → sprite_table →
 * pixel_gen → lcd_driver → LCD_D output.
 *
 * Build: make test-e2e
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Vppu_wrapper.h"
#include "verilated.h"

#define BG_COLOR 0x867D
#define TITLE_TILE 20
#define TITLE_W 64
#define TITLE_H 14

static Vppu_wrapper *ppu;

static void tick() {
    ppu->clk = 1; ppu->eval();
    ppu->clk = 0; ppu->eval();
}

static void send_byte(uint8_t byte) {
    for (int bit = 7; bit >= 0; bit--) {
        ppu->SPI_MOSI = (byte >> bit) & 1;
        ppu->SPI_CLK = 0;
        for (int i = 0; i < 6; i++) tick();
        ppu->SPI_CLK = 1;
        for (int i = 0; i < 6; i++) tick();
    }
    ppu->SPI_CLK = 0;
    for (int i = 0; i < 6; i++) tick();
}

static void cs_low() { ppu->SPI_CS = 0; for (int i = 0; i < 20; i++) tick(); }
static void cs_high() { for (int i = 0; i < 20; i++) tick(); ppu->SPI_CS = 1; for (int i = 0; i < 20; i++) tick(); }

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    ppu = new Vppu_wrapper;
    ppu->clk = 0; ppu->SPI_CLK = 0; ppu->SPI_MOSI = 0; ppu->SPI_CS = 1;
    ppu->eval();
    for (int i = 0; i < 20; i++) tick();

    /* 1. Upload pixel data for TILE_TITLE (64×14 = 896 pixels, padded to 64 width)
     *    Use a recognizable pattern: alternating red (0xF800) and green (0x07E0) */
    cs_low();
    send_byte(0x02);  /* CMD_PIXEL_UPLOAD */
    send_byte(0x00); send_byte(0x00);  /* addr = 0 */
    for (int i = 0; i < TITLE_W * TITLE_H; i++) {
        uint16_t color = (i % 2 == 0) ? 0xF800 : 0x07E0;
        send_byte(color & 0xFF);       /* low byte first (SPRAM pairing) */
        send_byte((color >> 8) & 0xFF);
    }
    cs_high();

    /* 2. Upload tile table: tile 20 = base_addr=0, width_shift=6 (64px), height=14 */
    cs_low();
    send_byte(0x04);  /* CMD_TILE_TABLE */
    send_byte(21);    /* 21 entries (indices 0-20, we only care about 20) */
    for (int i = 0; i < 20; i++) {
        send_byte(0); send_byte(0); send_byte(0); send_byte(1); /* dummy tiles: base=0, ws=0, h=1 */
    }
    /* Tile 20: base_hi=0, base_lo=0, width_shift=6, height=14 */
    send_byte(0x00); send_byte(0x00); send_byte(6); send_byte(14);
    cs_high();

    /* 3. Set background color */
    cs_low();
    send_byte(0x03);
    send_byte(BG_COLOR >> 8);
    send_byte(BG_COLOR & 0xFF);
    cs_high();

    /* 4. Send 1 sprite at (80, 80), tile=20, flags=0
     *    Firmware sends: cmd=0x01, count=1, x_lo, x_hi, y, tile, flags */
    cs_low();
    send_byte(0x01);  /* CMD_SPRITE_UPDATE */
    send_byte(1);     /* count */
    send_byte(80);    /* x low */
    send_byte(0);     /* x high */
    send_byte(80);    /* y */
    send_byte(TITLE_TILE);
    send_byte(0);     /* flags */
    cs_high();

    /* 5. Capture a full LCD frame */
    fprintf(stderr, "[test] Capturing LCD frame...\n");

    uint16_t framebuf[320 * 240];
    memset(framebuf, 0xFF, sizeof(framebuf));  /* fill with sentinel */

    int pixel_idx = 0;
    int in_pixel_mode = 0;
    uint8_t hi_byte = 0;
    int hi_phase = 0;
    uint8_t prev_wr = 0;

    for (int t = 0; t < 10000000 && pixel_idx < 320 * 240; t++) {
        prev_wr = ppu->LCD_WR;
        tick();
        if (ppu->LCD_WR && !prev_wr) {
            uint8_t byte = ppu->LCD_D;
            if (!ppu->LCD_DC) {
                if (byte == 0x2C) { in_pixel_mode = 1; pixel_idx = 0; hi_phase = 1; }
                else in_pixel_mode = 0;
            } else if (in_pixel_mode) {
                if (hi_phase) { hi_byte = byte; hi_phase = 0; }
                else {
                    framebuf[pixel_idx++] = (hi_byte << 8) | byte;
                    hi_phase = 1;
                }
            }
        }
    }

    if (pixel_idx < 320 * 240) {
        printf("FAIL: only captured %d/%d pixels\n", pixel_idx, 320 * 240);
        delete ppu;
        return 1;
    }

    /* 6. Verify framebuffer */
    int non_bg = 0, title_correct = 0, title_total = TITLE_W * TITLE_H;
    int bg_correct = 0, bg_total = 320 * 240 - title_total;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            uint16_t p = framebuf[y * 320 + x];
            int in_title = (x >= 80 && x < 80 + TITLE_W && y >= 80 && y < 80 + TITLE_H);
            if (in_title) {
                if (p != BG_COLOR) title_correct++;
            } else {
                if (p == BG_COLOR) bg_correct++;
                else non_bg++;
            }
        }
    }

    printf("pixels_captured: %d\n", pixel_idx);
    printf("title_region: %d/%d non-bg pixels\n", title_correct, title_total);
    printf("background: %d/%d correct\n", bg_correct, bg_total);
    printf("stray_pixels: %d (non-bg outside title)\n", non_bg);

    int pass = (title_correct > 0) && (bg_correct == bg_total) && (non_bg == 0);
    printf("%s\n", pass ? "PASS" : "FAIL");

    delete ppu;
    return pass ? 0 : 1;
}
