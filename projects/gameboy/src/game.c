/*
 * game.c — Jump game with dirty-rect rendering + debug timing
 */

#include "app.h"
#include "drivers/display.h"
#include "sched.h"

#define SCR_W 320
#define SCR_H 240
#define GROUND_Y 200
#define PLAYER_W 16
#define PLAYER_H 20
#define PLAYER_X 40
#define OBS_W 12
#define GRAVITY 1
#define JUMP_VEL (-12)
#define SCROLL_SPEED 3
#define MAX_OBS 3
#define FRAME_MS 33

extern uint32_t systick_get_ticks(void);

#define SKY     RGB565(30,30,50)
#define GROUND  RGB565(50,120,50)
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
    display_set_rotation(display, 0x28);

    /* Title screen */
    display_fill_rect(display, 0, 0, SCR_W, SCR_H, SKY);
    draw_string(112, 80, "DINO RUN", YELLOW);
    draw_string(64, 120, "PRESS A TO START", GREEN);
    display_vsync(display);
    button_pressed(0);
    while (!button_pressed(0)) sched_sleep_ms(50);

    for (;;) {
        int player_y = GROUND_Y - PLAYER_H;
        int vel_y = 0, on_ground = 1, score = 0, game_over = 0;
        int obs_x[MAX_OBS], obs_gap[MAX_OBS];
        int prev_player_y = player_y;
        int prev_obs_x[MAX_OBS];
        int prev_score = 0;

        for (int i = 0; i < MAX_OBS; i++) {
            obs_x[i] = SCR_W + 120 * i + (rng() % 80);
            obs_gap[i] = 20 + (rng() % 15);
            prev_obs_x[i] = obs_x[i];
        }

        display_fill_rect(display, 0, 0, SCR_W, GROUND_Y, SKY);
        display_fill_rect(display, 0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, GROUND);
        draw_string(SCR_W - 108, 4, "SCORE", WHITE);
        draw_number(SCR_W - 36, 4, 0, WHITE);

        while (!game_over) {
            uint32_t frame_start = systick_get_ticks();

            if (button_pressed(0) && on_ground) {
                vel_y = JUMP_VEL; on_ground = 0;
            }
            vel_y += GRAVITY;
            player_y += vel_y;
            if (player_y >= GROUND_Y - PLAYER_H) {
                player_y = GROUND_Y - PLAYER_H; vel_y = 0; on_ground = 1;
            }

            /* Player dirty-rect */
            if (player_y > prev_player_y) {
                int h = player_y - prev_player_y;
                display_fill_rect(display, PLAYER_X, prev_player_y, PLAYER_W, h, SKY);
            } else if (player_y < prev_player_y) {
                int h = prev_player_y - player_y;
                display_fill_rect(display, PLAYER_X, player_y + PLAYER_H, PLAYER_W, h, SKY);
            }
            display_fill_rect(display, PLAYER_X, player_y, PLAYER_W, PLAYER_H, YELLOW);
            display_fill_rect(display, PLAYER_X + 10, player_y + 5, 3, 3, BLACK);
            prev_player_y = player_y;

            /* Obstacles dirty-rect */
            for (int i = 0; i < MAX_OBS; i++) {
                int old_x = prev_obs_x[i];
                obs_x[i] -= SCROLL_SPEED;

                if (obs_x[i] < -OBS_W) {
                    /* Erase any remaining visible pixels */
                    int erase_x = old_x < 0 ? 0 : old_x;
                    int erase_right = old_x + OBS_W;
                    if (erase_right > SCR_W) erase_right = SCR_W;
                    int erase_w = erase_right - erase_x;
                    if (erase_w > 0 && erase_x < SCR_W) {
                        display_fill_rect(display, erase_x, GROUND_Y - obs_gap[i], erase_w, obs_gap[i], SKY);
                    }
                    obs_x[i] = SCR_W + (rng() % 40);
                    obs_gap[i] = 20 + (rng() % 15);
                    score++;
                    prev_obs_x[i] = obs_x[i];
                    continue;
                }

                int new_x = obs_x[i];
                int old_right = old_x + OBS_W;
                int new_right = new_x + OBS_W;

                /* Trail erase */
                if (old_right > new_right && old_right > 0 && new_right < SCR_W) {
                    int ex = new_right;
                    int ew = old_right - new_right;
                    if (ex < 0) { ew += ex; ex = 0; }
                    if (ex + ew > SCR_W) ew = SCR_W - ex;
                    if (ew > 0 && ex >= 0) {
                        display_fill_rect(display, ex, GROUND_Y - obs_gap[i], ew, obs_gap[i], SKY);
                    }
                }

                /* Draw obstacle */
                if (new_x < SCR_W && new_x + OBS_W > 0) {
                    int dx = new_x < 0 ? 0 : new_x;
                    int dw = OBS_W;
                    if (new_x < 0) dw += new_x;
                    if (dx + dw > SCR_W) dw = SCR_W - dx;
                    if (dw > 0) {
                        display_fill_rect(display, dx, GROUND_Y - obs_gap[i], dw, obs_gap[i], RED);
                    }
                }

                prev_obs_x[i] = new_x;
            }

            /* Score display */
            if (score > prev_score) {
                /* Erase old score area and redraw */
                display_fill_rect(display, SCR_W - 108, 4, 104, 16, SKY);
                draw_string(SCR_W - 108, 4, "SCORE", WHITE);
                draw_number(SCR_W - 36, 4, score, WHITE);
                prev_score = score;
            }

            if (check_collision(player_y, obs_x, obs_gap, MAX_OBS))
                game_over = 1;

            display_vsync(display);
            uint32_t elapsed = systick_get_ticks() - frame_start;
            if (elapsed < FRAME_MS)
                sched_sleep_ms(FRAME_MS - elapsed);
            else
                sched_yield();
        }

        display_fill_rect(display, 20, 55, 280, 120, RGB565(15,15,25));
        draw_string(106, 70, "GAME OVER", WHITE);
        draw_string(118, 100, "SCORE", WHITE);
        draw_number(190, 100, score, YELLOW);
        draw_string(34, 130, "PRESS A TO PLAY AGAIN", GREEN);
        display_vsync(display);
        button_pressed(0);
        while (!button_pressed(0)) sched_sleep_ms(100);
    }
}
