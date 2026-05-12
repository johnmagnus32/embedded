Split `game.c` into game logic and rendering. Game logic should have no display dependencies. Rendering should have no physics or game state mutation. Work in `/home/johmagnu/learning/simple-stm32/projects/gameboy/src`. Build with `make -C projects/gameboy`.

## Problem

`game.c` mixes game logic (physics, collision, scoring) with rendering (dirty rects, font drawing, display driver calls). This makes it:
- Hard to test game logic without a display
- Hard to swap rendering backends (e.g., switch to FPGA PPU later)
- Hard to read — 200 lines alternating between physics and pixel pushing

## Goal

```
game.c   — owns game state, physics, collision, state machine. No #include "drivers/display.h".
render.c — owns all drawing. Reads game state, never mutates it.
```

## New file: `game.h` (shared state definition)

```c
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define SCR_W       320
#define SCR_H       240
#define GROUND_Y    200
#define PLAYER_W    16
#define PLAYER_H    20
#define PLAYER_X    40
#define OBS_W       12
#define MAX_OBS     3
#define FRAME_MS    33

enum game_phase { PHASE_TITLE, PHASE_PLAYING, PHASE_GAME_OVER };

struct game_state {
    enum game_phase phase;
    int player_y;
    int vel_y;
    int on_ground;
    int score;
    int obs_x[MAX_OBS];
    int obs_gap[MAX_OBS];
};

/* game.c */
void game_init(struct game_state *s);
void game_update(struct game_state *s);  /* one tick of physics + input */

/* render.c */
void render_init(void);
void render_title(void);
void render_game_start(void);   /* clears screen, draws sky/ground/"SCORE" label */
void render_game_over(int score);
void render_frame(const struct game_state *cur, const struct game_state *prev);

#endif
```

## New file: `game.c` (logic only)

```c
#include "app.h"
#include "game.h"
#include "sched.h"

#define GRAVITY     1
#define JUMP_VEL    (-12)
#define SCROLL_SPEED 3

extern uint32_t systick_get_ticks(void);

static uint32_t rng_state = 12345;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

void game_init(struct game_state *s)
{
    s->phase = PHASE_PLAYING;
    s->player_y = GROUND_Y - PLAYER_H;
    s->vel_y = 0;
    s->on_ground = 1;
    s->score = 0;
    for (int i = 0; i < MAX_OBS; i++) {
        s->obs_x[i] = SCR_W + 120 * i + (rng() % 80);
        s->obs_gap[i] = 20 + (rng() % 15);
    }
}

void game_update(struct game_state *s)
{
    /* Input */
    if (button_pressed(BUTTON_A) && s->on_ground) {
        s->vel_y = JUMP_VEL;
        s->on_ground = 0;
    }

    /* Physics */
    s->vel_y += GRAVITY;
    s->player_y += s->vel_y;
    if (s->player_y >= GROUND_Y - PLAYER_H) {
        s->player_y = GROUND_Y - PLAYER_H;
        s->vel_y = 0;
        s->on_ground = 1;
    }

    /* Obstacles */
    for (int i = 0; i < MAX_OBS; i++) {
        s->obs_x[i] -= SCROLL_SPEED;
        if (s->obs_x[i] < -OBS_W) {
            s->obs_x[i] = SCR_W + (rng() % 40);
            s->obs_gap[i] = 20 + (rng() % 15);
            s->score++;
        }
    }

    /* Collision */
    for (int i = 0; i < MAX_OBS; i++) {
        if (s->obs_x[i] < PLAYER_X + PLAYER_W && s->obs_x[i] + OBS_W > PLAYER_X) {
            int obs_top = GROUND_Y - s->obs_gap[i];
            if (s->player_y + PLAYER_H > obs_top)
                s->phase = PHASE_GAME_OVER;
        }
    }
}

void game_task(void)
{
    render_init();

    for (;;) {
        /* Title screen */
        render_title();
        button_pressed(BUTTON_A);
        while (!button_pressed(BUTTON_A)) sched_sleep_ms(50);

        /* Game loop */
        struct game_state state, prev;
        game_init(&state);
        prev = state;

        render_game_start();            /* clear screen, draw sky/ground/"SCORE" label */
        render_frame(&state, &state);   /* draw initial player + obstacles */

        while (state.phase == PHASE_PLAYING) {
            uint32_t t0 = systick_get_ticks();
            prev = state;
            game_update(&state);
            render_frame(&state, &prev);

            uint32_t elapsed = systick_get_ticks() - t0;
            if (elapsed < FRAME_MS) sched_sleep_ms(FRAME_MS - elapsed);
            else sched_yield();
        }

        /* Game over */
        render_game_over(state.score);
        button_pressed(BUTTON_A);
        while (!button_pressed(BUTTON_A)) sched_sleep_ms(100);
    }
}
```

No `#include "drivers/display.h"`. No `display_fill_rect`. No font data. Pure logic.

## New file: `render.c` (display only)

```c
#include "app.h"
#include "game.h"
#include "drivers/display.h"

#define SKY     RGB565(30,30,50)
#define GROUND_C RGB565(50,120,50)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     RGB565(255,0,0)
#define GREEN   RGB565(0,255,0)
#define YELLOW  RGB565(255,255,0)

/* Font data + draw_char/draw_string/draw_number — moved here unchanged */
static const uint8_t font[][5] = { ... };
static void draw_char(int x, int y, char c, uint16_t color) { ... }
static void draw_string(int x, int y, const char *s, uint16_t color) { ... }
static void draw_number(int x, int y, int n, uint16_t color) { ... }

void render_init(void)
{
    display_set_rotation(display, 0x28);
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

        /* Erase trail */
        int old_right = old_x + OBS_W;
        int new_right = new_x + OBS_W;
        if (old_right > new_right && old_right > 0 && new_right < SCR_W) {
            int ex = new_right < 0 ? 0 : new_right;
            int ew = old_right - new_right;
            if (ex + ew > SCR_W) ew = SCR_W - ex;
            if (ew > 0) display_fill_rect(display, ex, GROUND_Y - gap, ew, gap, SKY);
        }

        /* Draw obstacle */
        if (new_x < SCR_W && new_x + OBS_W > 0) {
            int dx = new_x < 0 ? 0 : new_x;
            int dw = OBS_W + (new_x < 0 ? new_x : 0);
            if (dx + dw > SCR_W) dw = SCR_W - dx;
            if (dw > 0) display_fill_rect(display, dx, GROUND_Y - gap, dw, gap, RED);
        }
    }

    /* Score */
    if (cur->score > prev->score) {
        display_fill_rect(display, SCR_W - 36, 4, 32, 16, SKY);
        draw_number(SCR_W - 36, 4, cur->score, WHITE);
    }

    display_vsync(display);
}
```

`render_frame` takes current and previous state as `const` — it never mutates game state. It computes dirty rects by comparing the two.

## Key design decisions

**1. `render_frame` takes both current and previous state**

Instead of tracking `prev_player_y` and `prev_obs_x[]` as static variables inside the renderer, the game task passes both snapshots. This makes the renderer stateless (easier to test, no hidden state).

**2. Game state is a plain struct, not global**

Allows multiple game instances (unlikely but clean), and makes it easy to snapshot/restore for testing.

**3. The game task owns the frame timing**

`render_frame` doesn't sleep or wait — it just draws and returns. The game task decides when to call it and how long to sleep. This keeps timing control in one place.

## Changes summary

| File | Change |
|------|--------|
| `src/game.c` | REWRITE: remove all display code, keep physics/collision/state machine |
| `src/render.c` | NEW: all rendering logic moved here |
| `src/game.h` | NEW: shared `game_state` struct + function declarations |
| `src/app.h` | Remove `task_game` declaration (now in `game.h`), keep device externs |
| `Makefile` | Add `render.o` to object list |

## Testing

| Test | What it verifies |
|------|-----------------|
| Build | `make` succeeds with no warnings |
| Sim | Game runs in MCU sim, display output unchanged |
| game.c has no display include | `grep "display" src/game.c` returns nothing |
| render.c doesn't mutate state | All `game_state` params are `const *` |
| Collision still works | Play game, hit obstacle → game over triggers |
| Dirty rects correct | No visual artifacts (stale pixels, missing erases) |

## Future benefit

When switching to the FPGA PPU, `render.c` gets replaced entirely:

```c
// render_ppu.c — sends sprite positions instead of drawing pixels
void render_frame(const struct game_state *cur, const struct game_state *prev) {
    ppu_clear_sprites();
    ppu_add_sprite(PLAYER_X, cur->player_y, TILE_DINO, 0);
    for (int i = 0; i < MAX_OBS; i++)
        ppu_add_sprite(cur->obs_x[i], GROUND_Y - cur->obs_gap[i], TILE_CACTUS, 0);
    ppu_send_frame();
}
```

`game.c` stays identical — it doesn't know or care how rendering happens.
