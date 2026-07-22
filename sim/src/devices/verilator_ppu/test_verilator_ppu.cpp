#include <stdio.h>
#include "Vppu_wrapper.h"
#include "verilated.h"

static Vppu_wrapper *ppu;

static void tick() {
    ppu->clk = 1;
    ppu->eval();
    ppu->clk = 0;
    ppu->eval();
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

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    ppu = new Vppu_wrapper;

    ppu->clk = 0;
    ppu->SPI_CLK = 0;
    ppu->SPI_MOSI = 0;
    ppu->SPI_CS = 1;
    ppu->eval();

    // Idle ticks
    for (int i = 0; i < 20; i++) tick();

    // Assert CS
    ppu->SPI_CS = 0;
    for (int i = 0; i < 20; i++) tick();

    // Send BG_COLOR command (0x03) + 0xF8 + 0x00 = set bg to 0xF800
    send_byte(0x03);
    send_byte(0xF8);
    send_byte(0x00);

    // Settle
    for (int i = 0; i < 20; i++) tick();

    // Deassert CS
    ppu->SPI_CS = 1;
    for (int i = 0; i < 20; i++) tick();

    // Now check: run LCD until we see non-zero pixel output
    // The LCD driver starts from S_INIT, sends init commands, then pixel data.
    // With bg_color = 0xF800, all pixels should be 0xF800.
    int wr_count = 0;
    int got_pixel_data = 0;
    uint8_t last_dc = 0;
    uint8_t hi_byte = 0;
    uint16_t first_pixel = 0;
    int pixel_count = 0;

    int in_pixel_mode = 0;
    for (int i = 0; i < 2000000 && pixel_count < 10; i++) {
        uint8_t prev_wr = ppu->LCD_WR;
        tick();
        // Detect WR rising edge
        if (ppu->LCD_WR && !prev_wr) {
            wr_count++;
            if (!ppu->LCD_DC) {
                // Command byte — check for 0x2C (memory write)
                if (ppu->LCD_D == 0x2C) in_pixel_mode = 1;
                else in_pixel_mode = 0;
            } else if (in_pixel_mode) {
                // Data byte (pixel) — hi byte first, then lo byte
                if (got_pixel_data % 2 == 0) {
                    hi_byte = ppu->LCD_D;
                } else {
                    uint16_t pixel = (hi_byte << 8) | ppu->LCD_D;
                    if (pixel_count == 0) first_pixel = pixel;
                    pixel_count++;
                }
                got_pixel_data++;
            }
        }
    }

    printf("Verilator PPU test:\n");
    printf("  LCD_WR strobes: %d\n", wr_count);
    printf("  First pixel: 0x%04X (expected 0xF800)\n", first_pixel);
    printf("  Pixels captured: %d\n", pixel_count);

    if (first_pixel == 0xF800)
        printf("  >>> VERILATOR PASS: bg_color set correctly via SPI!\n");
    else if (first_pixel == 0x867D)
        printf("  >>> bg_color is default (SPI command not received)\n");
    else
        printf("  >>> bg_color is 0x%04X (unexpected)\n", first_pixel);

    delete ppu;
    return (first_pixel == 0xF800) ? 0 : 1;
}
