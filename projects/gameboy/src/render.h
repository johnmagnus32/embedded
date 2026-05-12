#ifndef RENDER_H
#define RENDER_H

#include "game.h"

void render_init(void);
void render_title(void);
void render_game_start(void);
void render_game_over(int score);
void render_frame(const struct game_state *cur, const struct game_state *prev);

#endif
