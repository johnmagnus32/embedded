/*
 * test_multi_frame.c — Send two sprite frames with different positions.
 * Tests that bank swap works through the full MCU→SPI→FPGA path.
 */
#include "gameboy_v2_test.h"

void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);

    /* Upload minimal tile data */
    PPU_CS_LOW();
    ppu_spi_byte(CMD_PIXEL_UPLOAD);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x00); ppu_spi_byte(0xF8);
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(5000);

    PPU_CS_LOW();
    ppu_spi_byte(CMD_TILE_TABLE); ppu_spi_byte(0x01);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x00); ppu_spi_byte(0x01);
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(5000);

    ppu_set_bg_color(0x0000);
    delay(5000);

    TEST("frame1_sprite_at_10_10");
    ppu_send_sprite_frame(1, 10, 0, 10, 0, 0);
    delay(100000);

    TEST("frame2_sprite_at_50_50");
    ppu_send_sprite_frame(1, 50, 0, 50, 0, 0);
    delay(100000);

    TEST("frame3_no_sprites");
    ppu_send_sprite_frame(0, 0, 0, 0, 0, 0);
    delay(100000);

    TEST_DONE("test_multi_frame");
}
