/*
 * game.c — Self-playing "Dino Run" demo for display bring-up.
 *
 * game_update() runs a tiny physics sim (gravity + auto-jump) with no button
 * input, so the PPU->ILI9341 path shows continuous animation on the bench: the
 * dino auto-hops over scrolling obstacles and the score counts up. game_task()
 * shows the title briefly, then loops update+render_frame at ~30 fps.
 *
 * (The real game will drive player_y from the jump button via buttons.c; this
 * demo just proves the render pipeline animates.)
 */
#include "board.h"
#include "game.h"
#include "render.h"
#include "sched.h"

/* Player sits at PLAYER_X; y is the sprite top. Ground level for the player's
 * feet is GROUND_Y, so the resting top is GROUND_Y - PLAYER_H. */
#define PLAYER_GROUND_Y   (GROUND_Y - PLAYER_H)
#define JUMP_VEL          (-12)   /* initial upward velocity (px/frame, y down) */
#define GRAVITY           (1)     /* downward accel per frame */
#define OBS_SPEED         (4)     /* obstacles scroll left this many px/frame */
#define OBS_SPACING       (140)   /* horizontal gap between obstacles */
#define JUMP_TRIGGER_X    (70)    /* auto-jump when nearest obstacle is this close ahead */

void game_init(struct game_state *s)
{
    s->phase = PHASE_PLAYING;
    s->player_y = PLAYER_GROUND_Y;
    s->vel_y = 0;
    s->on_ground = 1;
    s->score = 0;
    /* Stagger the three obstacles across (and past) the screen. */
    for (int i = 0; i < MAX_OBS; i++) {
        s->obs_x[i] = SCR_W + i * OBS_SPACING;
        s->obs_gap[i] = 0;                 /* sit on the ground */
    }
}

void game_update(struct game_state *s)
{
    /* --- vertical physics --- */
    if (!s->on_ground) {
        s->vel_y += GRAVITY;
        s->player_y += s->vel_y;
        if (s->player_y >= PLAYER_GROUND_Y) {
            s->player_y = PLAYER_GROUND_Y;
            s->vel_y = 0;
            s->on_ground = 1;
        }
    }

    /* --- scroll obstacles left; respawn off the right edge --- */
    for (int i = 0; i < MAX_OBS; i++) {
        s->obs_x[i] -= OBS_SPEED;
        if (s->obs_x[i] < -OBS_W) {
            /* respawn behind the farthest-right obstacle to keep spacing even */
            int max_x = 0;
            for (int j = 0; j < MAX_OBS; j++)
                if (s->obs_x[j] > max_x) max_x = s->obs_x[j];
            s->obs_x[i] = max_x + OBS_SPACING;
            s->score++;                    /* one point per cleared obstacle */
        }
    }

    /* --- auto-jump: hop if an obstacle is just ahead of the player --- */
    if (s->on_ground) {
        for (int i = 0; i < MAX_OBS; i++) {
            int ahead = s->obs_x[i] - PLAYER_X;
            if (ahead > 0 && ahead < JUMP_TRIGGER_X) {
                s->vel_y = JUMP_VEL;
                s->on_ground = 0;
                break;
            }
        }
    }
}

void game_task(void)
{
    uart_print("game: init\n");
    render_init();

    /* Show the title for ~1.5 s, then run the self-playing demo. */
    render_title();
    uart_print("game: title\n");
    for (int i = 0; i < 45; i++) sched_sleep_ms(FRAME_MS);

    struct game_state s;
    game_init(&s);
    uart_print("game: playing\n");

    uint32_t frame = 0;
    for (;;) {
        game_update(&s);
        render_frame(&s, &s);
        if ((frame++ & 63) == 0) {         /* periodic heartbeat on the console */
            uart_print("game: frame score=");
            print_int(s.score);
            uart_print("\n");
        }
        sched_sleep_ms(FRAME_MS);          /* ~30 fps */
    }
}
