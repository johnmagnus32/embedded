/* games/flappy/flappy.c — "Flappy": tap to flap a bird through gaps in scrolling pipes; touch or
 * hit a pipe/the ground and you're out. An ORIGINAL take on the flap-through-gaps genre (our own
 * bird sprite + sounds + plain pipe columns — none of the original game's art or branding).
 *
 * Exercises: fixed-timestep physics (gravity + impulse), the touch/pointer input, per-velocity
 * sprite tilt via eng_draw_image_cell (the 3-frame bird sheet), and the audio subsystem (flap /
 * score / hit SFX). Motion + collision live in fixed_update (stable, framerate-independent); input
 * EDGES are read in update() (an edge would re-fire on every physics sub-step otherwise).
 *
 * Controls: TAP the screen, or SPACE / A / UP, to flap. Tap on the game-over screen to retry.
 */
#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

enum { S_TITLE, S_PLAY, S_DEAD };

#define BIRD_R    13
#define CELL      40                 /* bird sheet cell size */
#define GROUND    72                 /* ground strip height at the bottom */
#define GAP       158.0f             /* vertical gap between the top + bottom pipe */
#define PIPE_W    74.0f
#define CAP_H     26                 /* pipe cap height (matches pipecap.png) */
#define SPACING   235.0f             /* horizontal distance between pipes */
#define SPEED     175.0f             /* pipe scroll speed (px/s) */
#define GRAV      1500.0f            /* gravity (px/s^2) */
#define FLAP      445.0f             /* upward impulse per flap (px/s) */
#define NPIPE     6

#define C_SKY    ENG_RGB(120, 200, 235)
#define C_PIPE   ENG_RGB(74, 182, 94)
#define C_PIPED  ENG_RGB(54, 150, 74)     /* pipe cap / shade */
#define C_PIPEO  ENG_RGB(38, 110, 56)     /* pipe outline */
#define C_GRND   ENG_RGB(222, 196, 140)
#define C_GRASS  ENG_RGB(120, 200, 96)
#define C_TXT    ENG_RGB(255, 255, 255)
#define C_SHAD   ENG_RGB(60, 90, 70)
#define C_GOLD   ENG_RGB(255, 220, 90)

typedef struct { float x, gap_y; int scored; } Pipe;

static int   W, H, st, score, best;
static float bird_x, bird_y, bird_vy, g_t, ground_scroll;
static Pipe  pipes[NPIPE];
static eng_image *img_bird, *img_pipe, *img_cap, *img_ground;
static eng_sound *sfx_flap, *sfx_score, *sfx_hit;

static float rnd_gap(void)
{
	float lo = GAP / 2 + 40, hi = (H - GROUND) - GAP / 2 - 40;
	return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

static void new_game(void)
{
	bird_x = W * 0.28f; bird_y = H * 0.5f; bird_vy = 0; score = 0; ground_scroll = 0;
	for (int i = 0; i < NPIPE; i++)
		pipes[i] = (Pipe){ W + 140 + i * SPACING, rnd_gap(), 0 };
	st = S_TITLE;
}

static void die(void)
{
	st = S_DEAD;
	if (score > best) best = score;
	eng_sound_play(sfx_hit, 0.8f);
}

static void flap(void) { bird_vy = -FLAP; eng_sound_play(sfx_flap, 0.6f); }

static int flapped(void)
{
	return eng_just_pressed(ENG_A) || eng_just_pressed(ENG_UP) || eng_pointer_just_pressed();
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	srand((unsigned)time(NULL));
	img_bird   = eng_image_from_png("flappy/bird.png");
	img_pipe   = eng_image_from_png("flappy/pipe.png");
	img_cap    = eng_image_from_png("flappy/pipecap.png");
	img_ground = eng_image_from_png("flappy/ground.png");
	sfx_flap  = eng_sound_load("sfx/flap.wav");
	sfx_score = eng_sound_load("sfx/score.wav");
	sfx_hit   = eng_sound_load("sfx/hit.wav");
	new_game();
}

static void on_update(float dt)
{
	g_t += dt;
	if (st == S_TITLE)      { if (flapped()) { st = S_PLAY; flap(); } }
	else if (st == S_PLAY)  { if (flapped()) flap(); }
	else /* S_DEAD */       { if (flapped()) new_game(); }
}

/* Bird box vs a pipe's two rects (above/below the gap). */
static int hits_pipe(const Pipe *p)
{
	float bx0 = bird_x - BIRD_R, bx1 = bird_x + BIRD_R, by0 = bird_y - BIRD_R, by1 = bird_y + BIRD_R;
	if (bx1 < p->x || bx0 > p->x + PIPE_W) return 0;      /* not horizontally overlapping this pipe */
	float top = p->gap_y - GAP / 2, bot = p->gap_y + GAP / 2;
	return (by0 < top) || (by1 > bot);                    /* above the gap top or below the gap bottom */
}

static void on_fixed(float fdt)
{
	if (st == S_TITLE) { bird_y = H * 0.5f + sinf(g_t * 4.0f) * 8.0f; return; }  /* gentle bob */
	if (st != S_PLAY) return;

	bird_vy += GRAV * fdt; bird_y += bird_vy * fdt;
	if (bird_y < BIRD_R) { bird_y = BIRD_R; if (bird_vy < 0) bird_vy = 0; }      /* ceiling: clamp, don't die */
	ground_scroll += SPEED * fdt;

	for (int i = 0; i < NPIPE; i++) {
		Pipe *p = &pipes[i];
		p->x -= SPEED * fdt;
		if (!p->scored && p->x + PIPE_W < bird_x) { p->scored = 1; score++; eng_sound_play(sfx_score, 0.5f); }
		if (p->x < -PIPE_W) { p->x += NPIPE * SPACING; p->gap_y = rnd_gap(); p->scored = 0; }  /* recycle right */
		if (hits_pipe(p)) { die(); return; }
	}
	if (bird_y + BIRD_R > H - GROUND) { bird_y = H - GROUND - BIRD_R; die(); }   /* ground */
}

static void draw_pipe(const Pipe *p)
{
	int x = (int)p->x, top = (int)(p->gap_y - GAP / 2), bot = (int)(p->gap_y + GAP / 2);
	int w = (int)PIPE_W, gy = H - GROUND;
	int x0 = x < 0 ? 0 : x, x1 = x + w > W ? W : x + w;   /* clip columns to the screen */
	int ty1 = top - CAP_H, by0 = bot + CAP_H;             /* bodies leave room for the caps */
	for (int sx = x0; sx < x1; sx++) {                    /* pipe body = pipe.png stretched vertically */
		int tx = sx - x;
		if (ty1 > 0)  eng_tex_column(img_pipe, sx, tx, 0, ty1, 256, 0);   /* top pipe */
		if (by0 < gy) eng_tex_column(img_pipe, sx, tx, by0, gy, 256, 0);  /* bottom pipe */
	}
	eng_draw_image(img_cap, x + w / 2, top - CAP_H / 2, ENG_WHITE);       /* caps (fixed height, wider) */
	eng_draw_image(img_cap, x + w / 2, bot + CAP_H / 2, ENG_WHITE);
}

static void on_draw_background(void)
{
	char buf[16];
	eng_clear(C_SKY);
	for (int i = 0; i < NPIPE; i++) draw_pipe(&pipes[i]);

	/* ground: a textured tile scrolled horizontally (flat-strip fallback if it didn't load) */
	int gy = H - GROUND;
	if (img_ground) {
		int gw = eng_image_w(img_ground); if (gw < 1) gw = 48;
		int off = (int)ground_scroll % gw;
		for (int x = -off; x < W + gw; x += gw)
			eng_draw_image(img_ground, x + gw / 2, gy + GROUND / 2, ENG_WHITE);
	} else {
		eng_rect_fill(0, gy, W, GROUND, C_GRND);
		eng_rect_fill(0, gy, W, 10, C_GRASS);
	}

	/* bird — tilt frame from vertical velocity (0 up / 1 level / 2 diving) */
	int fr = bird_vy < -60 ? 0 : bird_vy > 180 ? 2 : 1;
	if (st == S_TITLE) fr = 1;
	eng_draw_image_cell(img_bird, (int)bird_x, (int)bird_y, CELL, CELL, fr, 0, ENG_WHITE);

	if (st == S_PLAY) {                          /* live score, big + centered */
		snprintf(buf, sizeof buf, "%d", score);
		eng_text_aligned(W / 2 + 2, 34, 54, C_SHAD, ENG_ALIGN_CENTER, buf);
		eng_text_aligned(W / 2, 32, 54, C_TXT, ENG_ALIGN_CENTER, buf);
	}
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 96, 72, C_TXT, ENG_ALIGN_CENTER, "FLAPPY");
		eng_text_aligned(W / 2, H / 2 - 30, 26, C_TXT, ENG_ALIGN_CENTER, "TAP  /  SPACE  /  UP  to flap");
		eng_text_aligned(W / 2, H / 2 + 8, 24, C_TXT, ENG_ALIGN_CENTER, "fly through the gaps");
	} else if (st == S_DEAD) {
		char buf[32];
		eng_text_aligned(W / 2, H / 2 - 90, 68, C_TXT, ENG_ALIGN_CENTER, "GAME OVER");
		snprintf(buf, sizeof buf, "SCORE  %d", score);
		eng_text_aligned(W / 2, H / 2 - 18, 34, C_GOLD, ENG_ALIGN_CENTER, buf);
		snprintf(buf, sizeof buf, "BEST  %d", best);
		eng_text_aligned(W / 2, H / 2 + 22, 28, C_TXT, ENG_ALIGN_CENTER, buf);
		eng_text_aligned(W / 2, H / 2 + 70, 26, C_TXT, ENG_ALIGN_CENTER, "TAP TO RETRY");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Flappy", .init = on_init, .update = on_update,
		.fixed_update = on_fixed, .draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
