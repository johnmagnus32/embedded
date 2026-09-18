/* games/asteroids/asteroids.c — Asteroids: the game that exercises the engine's new AFFINE
 * rendering (node->rot / node->scale) and a MOMENTUM motion model. The ship rotates and thrusts
 * with inertia — velocity persists frame to frame and the screen wraps — while rocks drift and
 * split into smaller rocks when shot. Everything is a WORLD-layer node; there is NO camera (one
 * wrapping screen). Contrast with the other games' instant-velocity control: here input applies
 * ACCELERATION and velocity carries over, so you coast, drift, and fight your own momentum.
 *
 * Controls: LEFT/RIGHT rotate, UP thrust, A (z/Space) fire, START to play / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define PI 3.14159265f
enum { T_SHIP = 1, T_ROCK, T_BULLET };
enum { S_TITLE, S_PLAY, S_DEAD };

#define TURN     3.6f         /* ship turn rate (rad/s) */
#define THRUST   260.0f       /* thrust acceleration (px/s^2) */
#define MAXSPD   330.0f       /* ship speed cap (px/s) — bounds the coast without killing it */
#define BSPEED   480.0f       /* bullet speed (px/s) */
#define FIRE_CD  0.22f        /* seconds between shots while A is held */
#define ROCK_BASE 40          /* rock sprite is 40x40; node->scale picks the size tier */

/* palette */
#define C_SPACE     ENG_RGB(6, 8, 16)      /* deep space background */
#define C_STARS     ENG_RGB(120, 120, 140) /* static starfield dots */
#define C_TITLE     ENG_RGB(150, 220, 255) /* title text */
#define C_HELP      ENG_RGB(180, 190, 210) /* controls hint text */
#define C_GAMEOVER  ENG_RGB(255, 120, 120) /* game-over text */
#define C_PROMPT    ENG_RGB(230, 230, 240) /* press-start prompt */

static int   W, H, st;
static int   score, lives, wave;
static char  hud[64];
static float svx, svy, sang, fire_cd, respawn_t;   /* the ship's momentum state (only one ship) */
static eng_node  *ship, *hud_label;
static eng_image *img_ship, *img_rock, *img_bullet;

static float frand(float a, float b) { return a + (b - a) * ((float)rand() / (float)RAND_MAX); }

static void wrap(eng_node *n)               /* toroidal screen: leave one edge, enter the opposite */
{
	if (n->x < 0) n->x += W; else if (n->x >= W) n->x -= W;
	if (n->y < 0) n->y += H; else if (n->y >= H) n->y -= H;
}

static void bullet_update(eng_node *self, float dt)
{
	self->x += cosf(self->rot) * BSPEED * dt;   /* rot holds the firing direction */
	self->y += sinf(self->rot) * BSPEED * dt;
	if (self->x < 0 || self->x >= W || self->y < 0 || self->y >= H) eng_node_free(self);  /* off-screen */
}

static void rock_update(eng_node *self, float dt)
{
	float sp = 70.0f / self->scale;             /* smaller rocks drift faster */
	self->x += cosf(self->rot) * sp * dt;       /* rot = heading (also the render angle) */
	self->y += sinf(self->rot) * sp * dt;
	wrap(self);
}

static void spawn_bullet(float x, float y, float ang)
{
	eng_node *b = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_bullet);
	if (!b) return;
	b->x = x; b->y = y; b->rot = ang; b->z = 8;
	b->tag = T_BULLET; b->update = bullet_update;
}

static void spawn_rock(float x, float y, float scale)
{
	eng_node *r = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_rock);
	if (!r) return;
	r->x = x; r->y = y; r->scale = scale; r->rot = frand(0.0f, 2.0f * PI); r->z = 5;
	r->w = r->h = (int)(ROCK_BASE * scale * 0.82f);   /* AABB a touch inside the visual for fair hits */
	r->tag = T_ROCK; r->update = rock_update;
}

static void split_rock(eng_node *r)
{
	float sc = r->scale;
	score += (sc > 1.2f) ? 20 : (sc > 0.8f) ? 50 : 100;   /* smaller = worth more */
	if (sc > 0.8f) {                                       /* big/med break into two smaller rocks */
		float ns = (sc > 1.2f) ? 1.0f : 0.55f;
		spawn_rock(r->x, r->y, ns);
		spawn_rock(r->x, r->y, ns);
	}
	eng_node_free(r);
}

static void ship_update(eng_node *self, float dt)
{
	if (st != S_PLAY) return;
	if (respawn_t > 0) respawn_t -= dt;

	if (eng_pressed(ENG_LEFT))  sang -= TURN * dt;
	if (eng_pressed(ENG_RIGHT)) sang += TURN * dt;
	if (eng_pressed(ENG_UP)) { svx += cosf(sang) * THRUST * dt; svy += sinf(sang) * THRUST * dt; }

	float sp2 = svx * svx + svy * svy;                     /* cap speed (keeps the coast, bounds it) */
	if (sp2 > MAXSPD * MAXSPD) { float k = MAXSPD / sqrtf(sp2); svx *= k; svy *= k; }
	self->x += svx * dt; self->y += svy * dt;
	wrap(self);
	self->rot = sang;                                      /* the sprite points along its heading */

	fire_cd -= dt;
	if (eng_pressed(ENG_A) && fire_cd <= 0) {
		spawn_bullet(self->x + cosf(sang) * 18.0f, self->y + sinf(sang) * 18.0f, sang);   /* at the nose */
		fire_cd = FIRE_CD;
	}

	self->visible = (respawn_t > 0) ? (fmodf(respawn_t, 0.2f) < 0.1f) : true;   /* blink while invuln */

	if (respawn_t <= 0 && eng_overlap_next(self, T_ROCK, NULL)) {   /* struck a rock */
		if (--lives <= 0) { st = S_DEAD; return; }
		self->x = W / 2.0f; self->y = H / 2.0f; svx = svy = 0; sang = -PI / 2.0f; respawn_t = 2.0f;
	}
}

static void spawn_wave(void)
{
	int count = wave + 3;
	for (int i = 0; i < count; i++) {                      /* spawn along the edges, away from the ship */
		float x, y;
		if (rand() & 1) { x = frand(0, W); y = (rand() & 1) ? 0 : H; }
		else            { y = frand(0, H); x = (rand() & 1) ? 0 : W; }
		spawn_rock(x, y, 1.6f);
	}
}

static void start_game(void)
{
	eng_scene_clear();
	score = 0; lives = 3; wave = 1;
	ship = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_ship);
	ship->tag = T_SHIP; ship->update = ship_update; ship->z = 10;
	ship->w = ship->h = 18;                                /* forgiving hitbox (sprite is 32) */
	ship->x = W / 2.0f; ship->y = H / 2.0f; svx = svy = 0; sang = -PI / 2.0f;
	respawn_t = 1.5f; fire_cd = 0;
	hud_label = eng_label_new(eng_layer(ENG_LAYER_HUD), 26);
	hud_label->x = 16; hud_label->y = 12; hud_label->text = hud;
	spawn_wave();
	st = S_PLAY;
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	srand((unsigned)time(NULL));
	img_ship   = eng_image_from_png("asteroids/ship.png");
	img_rock   = eng_image_from_png("asteroids/rock.png");
	img_bullet = eng_image_from_png("asteroids/bullet.png");
	st = S_TITLE;
}

static void on_update(float dt)
{
	(void)dt;   /* motion lives in the node updates; nothing time-based here */
	if (st == S_TITLE) { if (eng_just_pressed(ENG_START)) start_game(); return; }
	if (st == S_DEAD)  { if (eng_just_pressed(ENG_START)) { eng_scene_clear(); ship = NULL; st = S_TITLE; } return; }

	/* bullet vs rock: each bullet kills the first rock it touches (iterator skips freed nodes) */
	for (eng_node *b = eng_first_child(NULL); b; b = eng_next(b)) {
		if (b->tag != T_BULLET || b->_freed) continue;
		eng_node *r = eng_overlap_next(b, T_ROCK, NULL);
		if (r) { eng_node_free(b); split_rock(r); }
	}

	int rocks = 0;                                          /* wave cleared -> next, bigger wave */
	for (eng_node *n = eng_first_child(NULL); n; n = eng_next(n))
		if (n->tag == T_ROCK && !n->_freed) rocks++;
	if (rocks == 0) { wave++; spawn_wave(); }

	snprintf(hud, sizeof hud, "SCORE %d    LIVES %d    WAVE %d", score, lives, wave);
}

static void on_draw_background(void)
{
	eng_clear(C_SPACE);                                    /* deep space */
	for (int i = 0; i < 64; i++)                            /* cheap static starfield (low entropy) */
		eng_rect_fill((i * 137) % W, (i * 251) % H, 1, 1, C_STARS);
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 70, 84, C_TITLE, ENG_ALIGN_CENTER, "ASTEROIDS");
		eng_text_aligned(W / 2, H / 2 + 20, 28, C_HELP, ENG_ALIGN_CENTER, "ROTATE - THRUST - FIRE");
		eng_text_aligned(W / 2, H / 2 + 60, 30, ENG_WHITE, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_DEAD) {
		eng_text_aligned(W / 2, H / 2 - 60, 84, C_GAMEOVER, ENG_ALIGN_CENTER, "GAME OVER");
		char b[48]; snprintf(b, sizeof b, "SCORE %d", score);
		eng_text_aligned(W / 2, H / 2 + 20, 40, ENG_WHITE, ENG_ALIGN_CENTER, b);
		eng_text_aligned(W / 2, H / 2 + 70, 28, C_PROMPT, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Asteroids", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
