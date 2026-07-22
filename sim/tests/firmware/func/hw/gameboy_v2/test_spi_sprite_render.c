/*
 * test_spi_sprite_render.c — Full pipeline: MCU SPI → FPGA → LCD → display
 * Sends pixel data, tile table, and sprite frame via SPI.
 * Then waits long enough for the FPGA to render past the sprite's Y position.
 * Exits with success — the test runner should verify non-bg pixels appeared.
 *
 * This specifically tests the gameboy_v2 machine's incremental SPI bit-banging
 * path (spi_to_fpga_transfer → queued bits in gameboy_v2_tick).
 */
#include "gameboy_v2_test.h"

void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);

    TEST("spi_sprite_render");

    /* Upload 2 pixels: 0xF800 (red), 0x07E0 (green) */
    PPU_CS_LOW();
    ppu_spi_byte(CMD_PIXEL_UPLOAD);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x00); ppu_spi_byte(0xF8);
    ppu_spi_byte(0xE0); ppu_spi_byte(0x07);
    ppu_spi_wait();
    PPU_CS_HIGH();
    delay(50000);

    /* Tile: base=0, width_shift=1 (2px wide), height=2 */
    PPU_CS_LOW();
    ppu_spi_byte(CMD_TILE_TABLE);
    ppu_spi_byte(0x01);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x01); ppu_spi_byte(0x02);
    ppu_spi_wait();
    PPU_CS_HIGH();
    delay(50000);

    /* Background */
    ppu_set_bg_color(0x867D);
    delay(50000);

    /* Sprite at (0, 5) — very early scanline so we don't have to wait long */
    ppu_send_sprite_frame(1, 0, 0, 5, 0, 0);

    /* Wait for rendering — sprite at y=5 should appear quickly */
    delay(500000);

    TEST_DONE("test_spi_sprite_render");
}
