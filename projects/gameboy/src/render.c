/*
 * render.c — All rendering (display driver calls, fonts, dirty rects)
 *
 * Reads game state via const pointers. Never mutates game state.
 */

#include "board.h"
#include "render.h"
#include "drivers/display.h"

#define SKY     RGB565(30,30,50)
#define GROUND_C RGB565(50,120,50)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     RGB565(255,0,0)
#define GREEN   RGB565(0,255,0)
#define YELLOW  RGB565(255,255,0)

/* 5x7 bitmap font */
static const uint8_t font[][5] = {
    [0]={0x3E,0x51,0x49,0x45,0x3E}, [1]={0x00,0x42,0x7F,0x40,0x00},
    [2]={0x42,0x61,0x51,0x49,0x46}, [3]={0x21,0x41,0x45,0x4B,0x31},
    [4]={0x18,0x14,0x12,0x7F,0x10}, [5]={0x27,0x45,0x45,0x45,0x39},
    [6]={0x3C,0x4A,0x49,0x49,0x30}, [7]={0x01,0x71,0x09,0x05,0x03},
    [8]={0x36,0x49,0x49,0x49,0x36}, [9]={0x06,0x49,0x49,0x29,0x1E},
    [10]={0x7E,0x11,0x11,0x11,0x7E}, [11]={0x7F,0x49,0x49,0x49,0x36},
    [12]={0x3E,0x41,0x41,0x41,0x22}, [13]={0x7F,0x41,0x41,0x22,0x1C},
    [14]={0x7F,0x49,0x49,0x49,0x41}, [15]={0x3E,0x41,0x49,0x49,0x3A},
    [16]={0x7F,0x02,0x0C,0x02,0x7F}, [17]={0x3E,0x41,0x41,0x41,0x3E},
    [18]={0x7E,0x09,0x09,0x09,0x06}, [19]={0x7F,0x09,0x19,0x29,0x46},
    [20]={0x26,0x49,0x49,0x49,0x32}, [21]={0x01,0x01,0x7F,0x01,0x01},
    [22]={0x1F,0x20,0x40,0x20,0x1F}, [23]={0x00,0x41,0x7F,0x41,0x00},
    [24]={0x7F,0x40,0x40,0x40,0x40}, [25]={0x7F,0x04,0x08,0x10,0x7F},
    [26]={0x07,0x08,0x70,0x08,0x07}, [27]={0x3F,0x40,0x40,0x40,0x3F},
};

static void draw_char(int x, int y, char c, uint16_t color)
{
    const uint8_t *glyph = 0;
    if (c >= '0' && c <= '9') glyph = font[c - '0'];
    else {
        static const char map[] = "ABCDEGMOPRSTVILNYU";
        static const int idx[] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
        for (int i = 0; map[i]; i++)
            if (c == map[i]) { glyph = font[idx[i]]; break; }
    }
    if (!glyph) return;
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (glyph[col] & (1 << row))
                display_fill_rect(display, x + col * 2, y + row * 2, 2, 2, color);
}

static void draw_string(int x, int y, const char *s, uint16_t color)
{
    while (*s) { draw_char(x, y, *s++, color); x += 12; }
}

static void draw_number(int x, int y, int n, uint16_t color)
{
    if (n >= 100) { draw_char(x, y, '0' + (n / 100) % 10, color); x += 12; }
    if (n >= 10)  { draw_char(x, y, '0' + (n / 10) % 10, color); x += 12; }
    draw_char(x, y, '0' + n % 10, color);
}

void render_init(void)
{
    display_set_rotation(display, 0x28);
    display_fill_rect(display, 0, 0, SCR_W, SCR_H, SKY);
    display_vsync(display);
    extern void display_on(const struct device *dev);
    display_on(display);
}

void render_title(void)
{
    display_fill_rect(display, 0, 0, SCR_W, SCR_H, SKY);
    draw_string(112, 80, "DINO RUN", YELLOW);
    draw_string(64, 120, "PRESS A TO START", GREEN);
    display_vsync(display);
}

void render_game_start(void)
{
    display_fill_rect(display, 0, 0, SCR_W, GROUND_Y, SKY);
    display_fill_rect(display, 0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, GROUND_C);
    draw_string(SCR_W - 108, 4, "SCORE", WHITE);
    draw_number(SCR_W - 36, 4, 0, WHITE);
    display_vsync(display);
}

void render_game_over(int score)
{
    display_fill_rect(display, 20, 55, 280, 120, RGB565(15,15,25));
    draw_string(106, 70, "GAME OVER", WHITE);
    draw_string(118, 100, "SCORE", WHITE);
    draw_number(190, 100, score, YELLOW);
    draw_string(34, 130, "PRESS A TO PLAY AGAIN", GREEN);
    display_vsync(display);
}

void render_frame(const struct game_state *cur, const struct game_state *prev)
{
    /* Player dirty-rect */
    if (cur->player_y > prev->player_y) {
        int h = cur->player_y - prev->player_y;
        display_fill_rect(display, PLAYER_X, prev->player_y, PLAYER_W, h, SKY);
    } else if (cur->player_y < prev->player_y) {
        int h = prev->player_y - cur->player_y;
        display_fill_rect(display, PLAYER_X, cur->player_y + PLAYER_H, PLAYER_W, h, SKY);
    }
    display_fill_rect(display, PLAYER_X, cur->player_y, PLAYER_W, PLAYER_H, YELLOW);
    display_fill_rect(display, PLAYER_X + 10, cur->player_y + 5, 3, 3, BLACK);

    /* Obstacles dirty-rect */
    for (int i = 0; i < MAX_OBS; i++) {
        int old_x = prev->obs_x[i];
        int new_x = cur->obs_x[i];
        int gap = cur->obs_gap[i];

        /* Obstacle wrapped — erase old position fully */
        if (new_x > old_x) {
            int ex = old_x < 0 ? 0 : old_x;
            int er = old_x + OBS_W;
            if (er > SCR_W) er = SCR_W;
            int ew = er - ex;
            if (ew > 0 && ex < SCR_W)
                display_fill_rect(display, ex, GROUND_Y - prev->obs_gap[i], ew, prev->obs_gap[i], SKY);
            continue;  /* new position is off-screen right, nothing to draw */
        }

        /* Trail erase */
        int old_right = old_x + OBS_W;
        int new_right = new_x + OBS_W;
        if (old_right > new_right && old_right > 0 && new_right < SCR_W) {
            int ex = new_right < 0 ? 0 : new_right;
            int ew = old_right - new_right;
            if (ex + ew > SCR_W) ew = SCR_W - ex;
            if (ew > 0)
                display_fill_rect(display, ex, GROUND_Y - gap, ew, gap, SKY);
        }

        /* Draw obstacle */
        if (new_x < SCR_W && new_x + OBS_W > 0) {
            int dx = new_x < 0 ? 0 : new_x;
            int dw = OBS_W + (new_x < 0 ? new_x : 0);
            if (dx + dw > SCR_W) dw = SCR_W - dx;
            if (dw > 0)
                display_fill_rect(display, dx, GROUND_Y - gap, dw, gap, RED);
        }
    }

    /* Score — only redraw the number when it changes */
    if (cur->score > prev->score) {
        display_fill_rect(display, SCR_W - 36, 4, 32, 16, SKY);
        draw_number(SCR_W - 36, 4, cur->score, WHITE);
    }

    display_vsync(display);
}
