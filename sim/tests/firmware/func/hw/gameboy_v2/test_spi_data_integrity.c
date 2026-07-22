/*
 * test_spi_data_integrity.c — Send CMD_BG_COLOR with known value,
 * then send a sprite frame. If SPI data is corrupted, the FPGA won't
 * render correctly and the LCD output will be wrong.
 * We verify by sending two different bg colors and checking the firmware
 * doesn't hang (the SPI peripheral completes both transfers).
 */
#include "gameboy_v2_test.h"

void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);

    TEST("spi_bg_color_0xF800");
    ppu_set_bg_color(0xF800);
    delay(10000);

    TEST("spi_bg_color_0x07E0");
    ppu_set_bg_color(0x07E0);
    delay(10000);

    TEST("spi_bg_color_0x001F");
    ppu_set_bg_color(0x001F);
    delay(10000);

    TEST("spi_pixel_upload_4bytes");
    PPU_CS_LOW();
    ppu_spi_byte(CMD_PIXEL_UPLOAD);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00);
    ppu_spi_byte(0xDE); ppu_spi_byte(0xAD);
    ppu_spi_byte(0xBE); ppu_spi_byte(0xEF);
    ppu_spi_wait();
    PPU_CS_HIGH();
    delay(10000);

    TEST("spi_tile_table_upload");
    PPU_CS_LOW();
    ppu_spi_byte(CMD_TILE_TABLE);
    ppu_spi_byte(0x02);
    ppu_spi_byte(0x00); ppu_spi_byte(0x00); ppu_spi_byte(0x01); ppu_spi_byte(0x02);
    ppu_spi_byte(0x00); ppu_spi_byte(0x04); ppu_spi_byte(0x02); ppu_spi_byte(0x04);
    ppu_spi_wait();
    PPU_CS_HIGH();
    delay(10000);

    TEST_DONE("test_spi_data_integrity");
}
