#include "gameboy_v2_test.h"
void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);
    TEST("sprite_visible");
    PPU_CS_LOW();
    ppu_spi_byte(CMD_PIXEL_UPLOAD);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x00); ppu_spi_byte(0xF8);
    ppu_spi_byte(0xE0); ppu_spi_byte(0x07);
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(10000);
    PPU_CS_LOW();
    ppu_spi_byte(CMD_TILE_TABLE); ppu_spi_byte(0x01);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0x01); ppu_spi_byte(0x02);
    ppu_spi_wait(); PPU_CS_HIGH();
    delay(10000);
    ppu_set_bg_color(0x867D);
    delay(10000);
    ppu_send_sprite_frame(1, 10, 0, 50, 0, 0);
    delay(500000);
    TEST_DONE("test_sprite_visible");
}
