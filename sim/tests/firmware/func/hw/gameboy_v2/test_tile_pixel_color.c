/*
 * test_tile_pixel_color.c — Upload specific pixel colors, place sprite,
 * verify the full upload→render pipeline doesn't corrupt data.
 * Tests SPRAM write path + tile table + sprite rendering.
 */
#include "gameboy_v2_test.h"

void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);

    TEST("upload_red_green_blue_pixels");
    /* Upload 3 pixels: red, green, blue */
    PPU_CS_LOW();
    ppu_spi_byte(CMD_PIXEL_UPLOAD);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);  /* addr 0 */
    ppu_spi_byte(0x00); ppu_spi_byte(0xF8);  /* 0xF800 red */
    ppu_spi_byte(0xE0); ppu_spi_byte(0x07);  /* 0x07E0 green */
    ppu_spi_byte(0x1F); ppu_spi_byte(0x00);  /* 0x001F blue */
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(10000);

    TEST("upload_tile_3x1");
    /* Tile: base=0, width_shift=2 (4 pixels wide), height=1 */
    PPU_CS_LOW();
    ppu_spi_byte(CMD_TILE_TABLE); ppu_spi_byte(0x01);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x02); ppu_spi_byte(0x01);
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(10000);

    TEST("set_bg_and_render");
    ppu_set_bg_color(0xFFFF);  /* white bg */
    delay(5000);
    ppu_send_sprite_frame(1, 20, 0, 20, 0, 0);
    delay(500000);

    TEST_DONE("test_tile_pixel_color");
}
