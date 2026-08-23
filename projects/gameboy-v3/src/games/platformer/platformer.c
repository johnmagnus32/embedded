/* games/platformer/platformer.c — a side-scrolling platformer: the game that puts the
 * engine's new pieces to work. The level is a TILEMAP node on the WORLD layer; the CAMERA
 * follows the player so the world scrolls while the HUD (a LABEL node on the HUD layer)
 * stays pinned. Gravity + jump + AABB-vs-tilemap collision live here in the player's update
 * (Godot's _process); the engine owns the layers, camera transform, and rendering.
 *
 * Controls: LEFT/RIGHT run, A (or B) jump, START to start / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

enum { T_PLAYER = 1, T_COIN, T_GOAL };      /* node groups */
enum { S_TITLE, S_PLAY, S_WIN };

#define GRAV  1600.0f
#define JUMP  620.0f
#define RUN   240.0f
#define FALL_MAX 900.0f
#define COYOTE_T 0.10f            /* jump still allowed this long after leaving the ground */
#define JBUF_T   0.10f            /* a jump press is remembered this long */

/* Level tiles: '#'/'=' are solid, colored below. The level itself loads from a DATA FILE
 * (platformer/level1.tmj, painted in Tiled) — design lives in data, not code. */
static const char      TILE_KEYS[]   = "#=";
static const eng_color TILE_COLORS[] = { ENG_RGB(84, 120, 64), ENG_RGB(150, 124, 96) };  /* # grass, = wood */

static int        st, W, H;
static int        coins, total_coins;
static int        level_w, TP = 32;         /* TP (tile size) is read from the loaded level */
static char       hud[32];
static float      spawn_x, spawn_y;
static float      pvx, pvy;                 /* player velocity */
static int        on_ground;
static float      coyote, jump_buf;         /* grace timers for robust jumping */
static eng_level  *level;
static eng_tilemap *map;
static eng_node   *player, *coin_label;
static eng_image  *img_player, *img_coin, *img_flag;

static void player_update(eng_node *self, float dt)
{
	if (st != S_PLAY) return;

	/* run + jump + gravity */
	pvx = 0;
	if (eng_pressed(ENG_LEFT))  pvx -= RUN;
	if (eng_pressed(ENG_RIGHT)) pvx += RUN;

	/* Jump with COYOTE TIME + JUMP BUFFERING. `on_ground` flickers on/off frame-to-frame
	 * (sub-pixel gravity vs the collision epsilon), so requiring it on the exact key-press
	 * frame drops ~half of taps. Instead: remember the press briefly (jump_buf), and allow a
	 * jump for a short grace window after last being grounded (coyote). Standard platformer fix. */
	if (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_B) || eng_just_pressed(ENG_UP)) jump_buf = JBUF_T;
	else if (jump_buf > 0) jump_buf -= dt;
	if (jump_buf > 0 && coyote > 0) { pvy = -JUMP; coyote = 0; jump_buf = 0; }

	pvy += GRAV * dt;
	if (pvy > FALL_MAX) pvy = FALL_MAX;

	/* move + resolve against the tilemap — the engine does the per-axis snapping now */
	int hit = eng_move_and_collide(self, pvx * dt, pvy * dt);
	if (hit & (ENG_COL_LEFT | ENG_COL_RIGHT)) pvx = 0;
	if (hit & (ENG_COL_UP | ENG_COL_DOWN))    pvy = 0;
	on_ground = (hit & ENG_COL_DOWN) != 0;
	coyote = on_ground ? COYOTE_T : (coyote > 0 ? coyote - dt : 0);   /* refresh/decay the grace window */
	if (self->y - self->h / 2 > H) { self->x = spawn_x; self->y = spawn_y; pvx = pvy = 0; }  /* fell in a pit */

	/* camera follows the player, clamped to the level bounds */
	float cx = self->x, half = W / 2.0f;
	if (cx < half) cx = half;
	if (cx > level_w - half) cx = level_w - half;
	eng_camera_set(cx, H / 2.0f);

	/* collect coins / reach the goal (deferred free = safe to free inside the iterator) */
	for (eng_node *c = eng_overlap_next(self, T_COIN, NULL); c; c = eng_overlap_next(self, T_COIN, c))
		{ eng_node_free(c); coins++; }
	if (eng_overlap_next(self, T_GOAL, NULL)) st = S_WIN;
}

static void build_level(void)
{
	eng_scene_clear();
	coins = total_coins = 0;

	eng_tilemap *bgmap = eng_level_bg(level);                          /* non-solid decorations, behind */
	if (bgmap) { eng_node *bgn = eng_tilemap_node(eng_layer(ENG_LAYER_WORLD), bgmap); bgn->z = -1; }

	eng_node *tm = eng_tilemap_node(eng_layer(ENG_LAYER_WORLD), map);   /* solid tiles, WORLD (0,0) */
	tm->z = 0;

	int nobj; const eng_object *obj = eng_level_objects(level, &nobj);  /* spawn placed entities */
	for (int i = 0; i < nobj; i++) {
		float x = obj[i].x, y = obj[i].y;
		switch (obj[i].name[0]) {
		case 'p':
			player = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_player);
			player->tag = T_PLAYER; player->update = player_update; player->z = 10;
			player->x = spawn_x = x; player->y = spawn_y = y;
			break;
		case 'c': {
			eng_node *co = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_coin);
			co->tag = T_COIN; co->z = 5; co->x = x; co->y = y; total_coins++;
			break;
		}
		case 'g': {
			eng_node *go = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_flag);
			go->tag = T_GOAL; go->z = 5; go->x = x; go->y = y;
			break;
		}
		}
	}

	coin_label = eng_label_new(eng_layer(ENG_LAYER_HUD), 28);   /* HUD layer: pinned to screen */
	coin_label->x = 20; coin_label->y = 16; coin_label->text = hud;

	pvx = pvy = 0; on_ground = 0; coyote = jump_buf = 0;
	eng_camera_set(spawn_x < W / 2.0f ? W / 2.0f : spawn_x, H / 2.0f);
	st = S_PLAY;
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	img_player = eng_image_from_png("platformer/player.png");
	img_coin   = eng_image_from_png("platformer/coin.png");
	img_flag   = eng_image_from_png("platformer/flag.png");
	level = eng_level_load("platformer/level1.tmj", 32, TILE_KEYS, TILE_COLORS);   /* Tiled map */
	map = eng_level_map(level);
	if (map) { TP = eng_tilemap_px(map); level_w = eng_tilemap_cols(map) * TP; }
	eng_set_tilemap(map);       /* the solid world for eng_move_and_collide */
	st = S_TITLE;
}

static void on_update(float dt)
{
	(void)dt;   /* per-frame timestep is used by the node updates; nothing to do here */
	if (st == S_TITLE) { if (eng_just_pressed(ENG_START)) build_level(); return; }
	if (st == S_WIN)   { if (eng_just_pressed(ENG_START)) { eng_scene_clear(); player = NULL; st = S_TITLE; } return; }
	snprintf(hud, sizeof hud, "COINS %d/%d", coins, total_coins);   /* the HUD label reads this buffer */
}

static void on_draw_background(void)      /* immediate, behind the scene layers */
{
	eng_clear(ENG_RGB(110, 170, 230));    /* sky */
}

static void center(const char *s, int y, int px, eng_color c)
{
	eng_text((W - eng_text_width(px, s)) / 2, y, px, c, s);
}

static void on_draw_overlay(void)         /* title / win text, above everything */
{
	char buf[32];
	if (st == S_TITLE) {
		center("PLATFORMER", H / 2 - 70, 84, ENG_RGB(40, 40, 60));
		center("PRESS START", H / 2 + 30, 36, ENG_RGB(20, 20, 30));
	} else if (st == S_WIN) {
		center("YOU WIN!", H / 2 - 70, 84, ENG_RGB(60, 180, 90));
		snprintf(buf, sizeof buf, "COINS %d/%d", coins, total_coins);
		center(buf, H / 2 + 16, 40, ENG_WHITE);
		center("PRESS START", H / 2 + 72, 30, ENG_RGB(230, 230, 240));
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Platformer", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
