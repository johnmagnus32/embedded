/*
 * ppu.c — SPI protocol driver for iCE40UP5K PPU
 */
#include "ppu.h"
#include "board.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"

#define CMD_SPRITE_UPDATE  0x01
#define CMD_PIXEL_UPLOAD   0x02
#define CMD_BG_COLOR       0x03
#define CMD_TILE_TABLE     0x04

extern const struct device *spi_ppu;
extern const struct device *dev_gpioa;

static void cs_low(void)  { gpio_pin_set(dev_gpioa, 4, 0); }
static void cs_high(void) { gpio_pin_set(dev_gpioa, 4, 1); }

void ppu_init(void)
{
    cs_high();
}

void ppu_upload_pixels(uint16_t addr, const uint8_t *data, int len)
{
    uint8_t hdr[3] = { CMD_PIXEL_UPLOAD, addr >> 8, addr & 0xFF };
    cs_low();
    spi_write(spi_ppu, hdr, 3);
    spi_write(spi_ppu, data, len);
    cs_high();
}

void ppu_upload_tile_table(const uint8_t *entries, int num_tiles)
{
    uint8_t hdr[2] = { CMD_TILE_TABLE, num_tiles };
    cs_low();
    spi_write(spi_ppu, hdr, 2);
    spi_write(spi_ppu, entries, num_tiles * 4);
    cs_high();
}

void ppu_set_bg_color(uint16_t rgb565)
{
    uint8_t cmd[3] = { CMD_BG_COLOR, rgb565 >> 8, rgb565 & 0xFF };
    cs_low();
    spi_write(spi_ppu, cmd, 3);
    cs_high();
}

void ppu_send_frame(const struct ppu_sprite *sprites, int count)
{
    uint8_t hdr[2] = { CMD_SPRITE_UPDATE, count };
    cs_low();
    spi_write(spi_ppu, hdr, 2);
    spi_write(spi_ppu, (const uint8_t *)sprites, count * sizeof(struct ppu_sprite));
    cs_high();
}
