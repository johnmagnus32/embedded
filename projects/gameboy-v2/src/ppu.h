#ifndef PPU_H
#define PPU_H

#include <stdint.h>

#define PPU_MAX_SPRITES   64
#define PPU_MAX_TILES     256
#define PPU_TRANSPARENT   0xF81F

struct ppu_sprite {
    uint16_t x;
    uint8_t  y;
    uint8_t  tile;
    uint8_t  flags;
} __attribute__((packed));

void ppu_init(void);
void ppu_upload_pixels(uint16_t addr, const uint8_t *data, int len);
void ppu_upload_tile_table(const uint8_t *entries, int num_tiles);
void ppu_set_bg_color(uint16_t rgb565);
void ppu_send_frame(const struct ppu_sprite *sprites, int count);

#endif
