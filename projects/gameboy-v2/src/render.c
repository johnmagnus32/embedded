/*
 * render.c — Sprite-based rendering via PPU
 *
 * Same render.h interface as gameboy v1, but sends sprite table
 * to the iCE40 PPU instead of drawing rects on the display.
 */
#include "render.h"
#include "ppu.h"
#include "board.h"
#include "tiles.h"

#define SKY_COLOR 0x867D

static struct ppu_sprite sprites[PPU_MAX_SPRITES];
static int num_sprites;

static void sprite_add(uint16_t x, uint8_t y, uint8_t tile, uint8_t flags)
{
    if (num_sprites < PPU_MAX_SPRITES)
        sprites[num_sprites++] = (struct ppu_sprite){x, y, tile, flags};
}

/* Embedded tile data (raw binaries via linker sections) */
extern const uint8_t _tiles_data_start[];
extern const uint8_t _tiles_data_end[];
extern const uint8_t _tile_table_start[];
extern const uint8_t _tile_table_end[];

void render_init(void)
{
    ppu_init();
    ppu_upload_pixels(0, _tiles_data_start,
                      (int)(_tiles_data_end - _tiles_data_start));
    ppu_upload_tile_table(_tile_table_start,
                          (int)(_tile_table_end - _tile_table_start) / 4);
    ppu_set_bg_color(SKY_COLOR);
}

void render_title(void)
{
    num_sprites = 0;
    sprite_add(80, 80, TILE_TITLE, 0);
    ppu_send_frame(sprites, num_sprites);
}

void render_game_start(void)
{
    num_sprites = 0;
    ppu_send_frame(sprites, num_sprites);
}

void render_game_over(int score)
{
    num_sprites = 0;
    sprite_add(80, 60, TILE_GAMEOVER, 0);
    int x = 160;
    if (score >= 100) { sprite_add(x, 100, TILE_DIGIT_0 + (score/100)%10, 0); x += 12; }
    if (score >= 10)  { sprite_add(x, 100, TILE_DIGIT_0 + (score/10)%10, 0); x += 12; }
    sprite_add(x, 100, TILE_DIGIT_0 + score%10, 0);
    ppu_send_frame(sprites, num_sprites);
}

void render_frame(const struct game_state *cur, const struct game_state *prev)
{
    (void)prev;
    num_sprites = 0;

    /* Player */
    sprite_add(PLAYER_X, cur->player_y, TILE_PLAYER, 0);

    /* Obstacles — rest ON the ground (feet at GROUND_Y) on their own scanlines
     * ABOVE the ground strip, so they don't merge with the ground row. */
    for (int i = 0; i < MAX_OBS; i++) {
        if (cur->obs_x[i] >= 0 && cur->obs_x[i] < SCR_W)
            sprite_add(cur->obs_x[i], GROUND_Y - TILE_OBS_0_H, TILE_OBS_0, 0);
    }

    /* Score digits */
    int score = cur->score;
    int x = SCR_W - 48;
    if (score >= 100) { sprite_add(x, 4, TILE_DIGIT_0 + (score/100)%10, 0); x += 12; }
    if (score >= 10)  { sprite_add(x, 4, TILE_DIGIT_0 + (score/10)%10, 0); x += 12; }
    sprite_add(x, 4, TILE_DIGIT_0 + score%10, 0);

    /* Ground: a solid band from the ground line to the bottom of the screen,
     * tiled across the width. TILE_GROUND_FILL is 16x40, so at y=GROUND_Y=200 it
     * fills 200..239 (the whole bottom). (TILE_GROUND is only 4px tall — a thin
     * line — so it's not used for the play field.) */
    for (int i = 0; i < SCR_W / TILE_GROUND_FILL_W; i++)
        sprite_add(i * TILE_GROUND_FILL_W, GROUND_Y, TILE_GROUND_FILL, 0);

    ppu_send_frame(sprites, num_sprites);
}
