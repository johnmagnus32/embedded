/* games/shmup/shmup.c — a vertical shoot-'em-up, written the Godot/Unity-esque way.
 *
 * The player, each bullet, and each enemy are scene NODES. A node owns its behavior in an
 * update() callback (Godot's _process) and a `tag` (its group). The engine renders their
 * sprites and reaps freed ones; this file just spawns nodes, wires their behavior, and does
 * collision in the global update(). There is no manual per-object draw loop — that's the
 * point of the retained scene graph. Sprites are PNG files loaded at runtime (on_init).
 *
 * Controls: LEFT/RIGHT move, A (or B) fires, START to start / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* node groups (tags) */
enum { T_SHIP = 1, T_BULLET, T_ENEMY };

/* game states */
enum { S_TITLE, S_PLAY, S_OVER };

static int       st;
static int       W, H;
static int       score, lives;
static float     spawn_cd;          /* seconds until the next enemy */
static float     fire_cd;           /* player fire cooldown */
static eng_node *player;            /* valid only during S_PLAY */
static eng_image *img_ship, *img_bullet, *img_enemy;

/* small xorshift RNG so enemy spawns vary without pulling in libc rand() */
static uint32_t rng = 0x1234567u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* ---- scrolling starfield (background flavor, drawn in on_draw) ---- */
#define NSTAR 64
static float star_x[NSTAR], star_y[NSTAR], star_v[NSTAR];

static void stars_init(void)
{
	for (int i = 0; i < NSTAR; i++) {
		star_x[i] = (float)(rnd() % (uint32_t)W);
		star_y[i] = (float)(rnd() % (uint32_t)H);
		star_v[i] = 30.0f + (float)(rnd() % 130u);
	}
}

static void stars_step(float dt)
{
	for (int i = 0; i < NSTAR; i++) {
		star_y[i] += star_v[i] * dt;
		if (star_y[i] >= H) { star_y[i] = 0; star_x[i] = (float)(rnd() % (uint32_t)W); }
	}
}

/* ---- node behaviors (Godot _process) ---- */

static void bullet_update(eng_node *self, float dt)
{
	self->y -= 560.0f * dt;
	if (self->y < -8) eng_node_free(self);
}

static void enemy_update(eng_node *self, float dt)
{
	self->y += 135.0f * dt;
	self->x += sinf(self->y * 0.02f) * 55.0f * dt;   /* gentle weave */
	if (self->y > H + 20) eng_node_free(self);
}

static void player_update(eng_node *self, float dt)
{
	float sp = 400.0f;
	if (eng_pressed(ENG_LEFT))  self->x -= sp * dt;
	if (eng_pressed(ENG_RIGHT)) self->x += sp * dt;
	if (self->x < self->w / 2.0f)      self->x = self->w / 2.0f;
	if (self->x > W - self->w / 2.0f)  self->x = W - self->w / 2.0f;

	if (fire_cd > 0) fire_cd -= dt;
	if ((eng_pressed(ENG_A) || eng_pressed(ENG_B)) && fire_cd <= 0) {
		eng_node *b = eng_sprite_new(NULL, img_bullet);
		if (b) { b->tag = T_BULLET; b->update = bullet_update; b->z = 5;
		         b->x = self->x; b->y = self->y - self->h / 2.0f - 4; }
		fire_cd = 0.16f;
	}
}

/* ---- lifecycle ---- */

static void start_game(void)
{
	eng_scene_clear();
	score = 0; lives = 3; spawn_cd = 0.6f; fire_cd = 0;
	player = eng_sprite_new(NULL, img_ship);
	player->tag = T_SHIP; player->update = player_update; player->z = 10;
	player->x = W / 2.0f; player->y = H - 64;
	st = S_PLAY;
}

static void spawn_enemy(void)
{
	eng_node *e = eng_sprite_new(NULL, img_enemy);
	if (!e) return;
	e->tag = T_ENEMY; e->update = enemy_update; e->z = 5;
	e->x = (float)(e->w / 2 + (int)(rnd() % (uint32_t)(W - e->w)));
	e->y = (float)(-e->h);
}

static void on_update(float dt)
{
	stars_step(dt);

	if (st == S_TITLE) { if (eng_just_pressed(ENG_START)) start_game(); return; }
	if (st == S_OVER)  { if (eng_just_pressed(ENG_START)) { eng_scene_clear(); player = NULL; st = S_TITLE; } return; }

	/* S_PLAY: spawn a steady stream of enemies */
	spawn_cd -= dt;
	if (spawn_cd <= 0) { spawn_enemy(); spawn_cd = 0.55f; }

	/* Collisions via the group-overlap iterator. Frees are deferred (queue_free) and the
	 * iterator skips freed nodes, so it's safe to free hits as we go. Each bullet dies on its
	 * first enemy; enumerate bullets with the tree walk (there's no overlap probe for "all
	 * bullets"), then ask the iterator for an enemy it touches. */
	for (eng_node *b = eng_first_child(NULL); b; b = eng_next(b)) {
		if (b->tag != T_BULLET || b->_freed) continue;
		eng_node *e = eng_overlap_next(b, T_ENEMY, NULL);
		if (e) { eng_node_free(b); eng_node_free(e); score += 10; }
	}

	int died = 0;
	if (player)
		for (eng_node *e = eng_overlap_next(player, T_ENEMY, NULL); e; e = eng_overlap_next(player, T_ENEMY, e))
			{ eng_node_free(e); if (--lives <= 0) died = 1; }
	if (died) { eng_scene_clear(); player = NULL; st = S_OVER; }   /* after the loop */
}

/* ---- rendering: background + HUD; the engine draws the sprites in between ---- */

static void on_draw_background(void)      /* immediate, before the scene layers */
{
	eng_clear(ENG_RGB(8, 10, 26));
	for (int i = 0; i < NSTAR; i++)
		eng_rect_fill((int)star_x[i], (int)star_y[i], 2, 2, ENG_RGB(120, 130, 170));
}

static void center(const char *s, int y, int px, eng_color c)
{
	eng_text((W - eng_text_width(px, s)) / 2, y, px, c, s);
}

static void on_draw_overlay(void)         /* HUD + overlays, after the scene layers */
{
	char buf[32];
	if (st == S_PLAY) {
		snprintf(buf, sizeof buf, "SCORE %d", score);
		eng_text(20, 16, 26, ENG_WHITE, buf);
		snprintf(buf, sizeof buf, "LIVES %d", lives);
		eng_text(W - 20 - eng_text_width(26, buf), 16, 26, ENG_WHITE, buf);
	} else if (st == S_TITLE) {
		center("SHMUP", H / 2 - 90, 96, ENG_RGB(120, 200, 255));
		center("PRESS START", H / 2 + 30, 40, ENG_WHITE);
	} else if (st == S_OVER) {
		center("GAME OVER", H / 2 - 74, 80, ENG_RGB(240, 80, 80));
		snprintf(buf, sizeof buf, "SCORE %d", score);
		center(buf, H / 2 + 14, 44, ENG_WHITE);
		center("PRESS START", H / 2 + 74, 30, ENG_RGB(180, 180, 200));
	}
}

static void on_init(void)
{
	W = eng_width();
	H = eng_height();
	img_ship   = eng_image_from_png("shmup/ship.png");
	img_enemy  = eng_image_from_png("shmup/enemy.png");
	img_bullet = eng_image_from_png("shmup/bullet.png");
	stars_init();
	st = S_TITLE;
}

int main(void)
{
	static const eng_game game = {
		.title = "Shmup", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
