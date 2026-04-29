/*
 * game.c — Jump game using display driver API
 *
 * No register addresses. All display via display_fill_rect/display_vsync.
 * Button input via input_is_pressed.
 */

#include "app.h"
#include "drivers/display.h"
#include "drivers/input.h"
#include "sched.h"

#define SCR_W 320
#define SCR_H 240
#define GROUND_Y 200
#define PLAYER_W 16
#define PLAYER_H 20
#define PLAYER_X 40
#define OBS_W 12
#define OBS_H 30
#define GRAVITY 1
#define JUMP_VEL (-12)
#define SCROLL_SPEED 3
#define MAX_OBS 3

#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     RGB565(255,0,0)
#define GREEN   RGB565(0,255,0)
#define YELLOW  RGB565(255,255,0)

static uint32_t rng_state = 12345;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* 5x7 bitmap font */
static const uint8_t font[][5] = {
    [0]={0x3E,0x51,0x49,0x45,0x3E}, [1]={0x00,0x42,0x7F,0x40,0x00},
    [2]={0x42,0x61,0x51,0x49,0x46}, [3]={0x21,0x41,0x45,0x4B,0x31},
    [4]={0x18,0x14,0x12,0x7F,0x10}, [5]={0x27,0x45,0x45,0x45,0x39},
    [6]={0x3C,0x4A,0x49,0x49,0x30}, [7]={0x01,0x71,0x09,0x05,0x03},
    [8]={0x36,0x49,0x49,0x49,0x36}, [9]={0x06,0x49,0x49,0x29,0x1E},
    [10]={0x7E,0x11,0x11,0x11,0x7E}, /* A */
    [11]={0x7F,0x49,0x49,0x49,0x36}, /* B */
    [12]={0x3E,0x41,0x41,0x41,0x22}, /* C */
    [13]={0x7F,0x41,0x41,0x22,0x1C}, /* D */
    [14]={0x7F,0x49,0x49,0x49,0x41}, /* E */
    [15]={0x3E,0x41,0x49,0x49,0x3A}, /* G */
    [16]={0x7F,0x04,0x08,0x10,0x7F}, /* M */
    [17]={0x3E,0x41,0x41,0x41,0x3E}, /* O */
    [18]={0x7E,0x09,0x09,0x09,0x06}, /* P */
    [19]={0x7F,0x09,0x19,0x29,0x46}, /* R */
    [20]={0x26,0x49,0x49,0x49,0x32}, /* S */
    [21]={0x01,0x01,0x7F,0x01,0x01}, /* T */
    [22]={0x3F,0x40,0x40,0x40,0x3F}, /* V */
    [23]={0x00,0x41,0x7F,0x41,0x00}, /* I */
    [24]={0x7F,0x40,0x40,0x40,0x40}, /* L */
    [25]={0x7F,0x04,0x08,0x10,0x7F}, /* N */
    [26]={0x07,0x08,0x70,0x08,0x07}, /* Y */
};

static void draw_char(int x, int y, char c, uint16_t color)
{
    const uint8_t *glyph = 0;
    if (c >= '0' && c <= '9') glyph = font[c - '0'];
    else {
        static const char map[] = "ABCDEGMOPRSTVILNY";
        static const int idx[] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26};
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

static int check_collision(int py, int obs_x[], int obs_gap[], int n)
{
    for (int i = 0; i < n; i++) {
        if (obs_x[i] < PLAYER_X + PLAYER_W && obs_x[i] + OBS_W > PLAYER_X) {
            int obs_top = GROUND_Y - obs_gap[i];
            if (py + PLAYER_H > obs_top) return 1;
        }
    }
    return 0;
}

void task_game(void)
{
    uart_print("game: init\n");
    display_set_rotation(display, 0x20);

    for (;;) {
        int player_y = GROUND_Y - PLAYER_H;
        int vel_y = 0, on_ground = 1, score = 0, game_over = 0;
        int obs_x[MAX_OBS], obs_gap[MAX_OBS];

        for (int i = 0; i < MAX_OBS; i++) {
            obs_x[i] = SCR_W + 120 * i + (rng() % 80);
            obs_gap[i] = 20 + (rng() % 15);
        }

        display_fill_rect(display, 0, 0, SCR_W, GROUND_Y, RGB565(30,30,50));
        display_fill_rect(display, 0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, RGB565(50,120,50));

        while (!game_over) {
            if (input_is_pressed(buttons, 0) && on_ground) {
                vel_y = JUMP_VEL; on_ground = 0;
            }
            vel_y += GRAVITY;
            player_y += vel_y;
            if (player_y >= GROUND_Y - PLAYER_H) {
                player_y = GROUND_Y - PLAYER_H; vel_y = 0; on_ground = 1;
            }

            display_fill_rect(display, PLAYER_X, 0, PLAYER_W, GROUND_Y, RGB565(30,30,50));
            display_fill_rect(display, PLAYER_X, player_y, PLAYER_W, PLAYER_H, YELLOW);
            display_fill_rect(display, PLAYER_X + 10, player_y + 5, 3, 3, BLACK);

            for (int i = 0; i < MAX_OBS; i++) {
                if (obs_x[i] >= 0 && obs_x[i] < SCR_W)
                    display_fill_rect(display, obs_x[i], 0, OBS_W, GROUND_Y, RGB565(30,30,50));
                obs_x[i] -= SCROLL_SPEED;
                if (obs_x[i] < -OBS_W) {
                    obs_x[i] = SCR_W + 60 + (rng() % 120);
                    obs_gap[i] = 20 + (rng() % 15);
                    score++;
                }
                if (obs_x[i] >= 0 && obs_x[i] < SCR_W)
                    display_fill_rect(display, obs_x[i], GROUND_Y - obs_gap[i], OBS_W, obs_gap[i], RED);
            }

            if (check_collision(player_y, obs_x, obs_gap, MAX_OBS))
                game_over = 1;

            display_vsync(display);
            sched_sleep_ms(33);
        }

        /* Game over screen */
        display_fill_rect(display, 30, 55, 260, 120, RGB565(15,15,25));
        draw_string(106, 70, "GAME OVER", WHITE);
        draw_string(118, 95, "SCORE", WHITE);
        draw_number(190, 95, score, YELLOW);
        draw_string(46, 120, "PRESS A TO PLAY AGAIN", GREEN);
        display_vsync(display);
        while (!input_is_pressed(buttons, 0)) sched_sleep_ms(33);
        while (input_is_pressed(buttons, 0)) sched_sleep_ms(33);
    }
}
