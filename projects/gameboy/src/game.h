/*
 * game.h — Game state and constants shared between logic and rendering
 */

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
void game_update(struct game_state *s);
void game_task(void);

#endif
