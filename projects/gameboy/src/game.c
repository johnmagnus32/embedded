/*
 * game.c — Game logic (physics, collision, state machine)
 *
 * No display code. No #include "drivers/display.h".
 */

#include "board.h"
#include "buttons.h"
#include "game.h"
#include "render.h"
#include "sched.h"

#define GRAVITY      1
#define JUMP_VEL     (-12)
#define SCROLL_SPEED 3

extern uint32_t systick_get_ticks(void);

static uint32_t rng_state = 12345;
static uint32_t rng(void)
{
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
    uart_print("game: init\n");
    render_init();

    for (;;) {
        /* Title screen */
        render_title();
        wait_for_button_press(BUTTON_A);

        /* Game loop */
        struct game_state state, prev;
        game_init(&state);
        prev = state;

        render_game_start();

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
        wait_for_button_press(BUTTON_A);
    }
}
