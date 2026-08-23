/* games/adventure/adventure.c — a top-down action-adventure: the game that exercises the
 * engine's newest muscle. The hero is an ANIMATED SPRITE (eng_anim_new: a sheet with one row
 * per facing direction and a walk cycle across the columns); slimes chase and are culled with
 * a sword swing; a key unlocks the goal chest. Enemies/pickups/goal are all found with the new
 * eng_overlap_next() group iterator instead of a hand-rolled child walk. The world is a
 * top-down TILEMAP (a ground layer + a solid obstacle layer) painted in data (adventure/
 * level1.tmj); the CAMERA follows the hero on BOTH axes, clamped to the level; the HUD (HP +
 * keys) is a screen-fixed LABEL node. Movement is eng_move_and_collide with no gravity —
 * the same per-axis tile resolve the platformer uses, just driven straight from the D-pad.
 *
 * Controls: arrows move, A (z / Space) swing the sword, START to start / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

enum { T_PLAYER = 1, T_SLIME, T_SWORD, T_HEART, T_KEY, T_CHEST };   /* node groups */
enum { S_TITLE, S_PLAY, S_WIN, S_DEAD };
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };                     /* == hero sheet rows */

#define SPD        150.0f     /* hero run speed (px/s) */
#define SLIME_SPD   58.0f
#define CHASE_R    240.0f     /* a slime only chases the hero within this radius */
#define SWORD_LIFE   0.18f    /* seconds the swing hitbox stays out */
#define SWORD_CD     0.34f    /* min seconds between swings */
#define INV_TIME     0.9f     /* i-frames after taking a hit */
#define MAXHP        5

/* Fallback flat-tile palette (used only if the .tmj's atlas image fails to load); gid n ->
 * TILE_COLORS[n-1]. Order matches tools/gen_adventure_assets.py: grass/path/flower/sand/
 * water/tree/wall. */
static const char      TILE_KEYS[]   = "1234567";
static const eng_color TILE_COLORS[] = {
	ENG_RGB(86,150,78), ENG_RGB(196,170,120), ENG_RGB(96,170,90), ENG_RGB(220,204,152),
	ENG_RGB(64,118,196), ENG_RGB(44,112,52), ENG_RGB(120,120,132),
};

static int         st, W, H;                 /* game state; screen size */
static int         level_w, level_h, TP = 32;
static char        hud[40];
static float       spawn_x, spawn_y;
static int         facing, hp, keys;
static float       attack_cd, inv, hint_t;   /* swing cooldown, i-frames, "need a key" hint */
static eng_level  *level;
static eng_tilemap *map;
static eng_node   *player, *hud_label, *sword;
static float       sword_t;
static eng_image  *img_hero, *img_slime, *img_sword, *img_heart, *img_key, *img_chest;

static void camera_follow(float cx, float cy)   /* center on (cx,cy), clamped to the level */
{
	float hw = W / 2.0f, hh = H / 2.0f;
	if (cx < hw) cx = hw;
	if (cx > level_w - hw) cx = level_w - hw;
	if (cy < hh) cy = hh;
	if (cy > level_h - hh) cy = level_h - hh;
	eng_camera_set(cx, cy);
}

static void hurt_player(void)
{
	if (--hp <= 0) st = S_DEAD;
	inv = INV_TIME;
}

static void swing_sword(void)   /* spawn the short-lived swing hitbox in front of the hero */
{
	sword = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_sword);
	if (!sword) return;
	sword->tag = T_SWORD; sword->z = 12; sword->w = 26; sword->h = 26;
	float off = 24.0f;
	sword->x = player->x + (facing == DIR_RIGHT ? off : facing == DIR_LEFT ? -off : 0);
	sword->y = player->y + (facing == DIR_DOWN  ? off : facing == DIR_UP   ? -off : 0);
	sword_t = SWORD_LIFE;
}

static void player_update(eng_node *self, float dt)
{
	if (st != S_PLAY) return;

	/* read the D-pad -> a direction vector */
	float dx = 0, dy = 0;
	if (eng_pressed(ENG_LEFT))  dx -= 1;
	if (eng_pressed(ENG_RIGHT)) dx += 1;
	if (eng_pressed(ENG_UP))    dy -= 1;
	if (eng_pressed(ENG_DOWN))  dy += 1;

	/* facing + walk animation (idle -> hold the standing frame) */
	if (dx != 0 || dy != 0) {
		facing = (fabsf(dx) >= fabsf(dy)) ? (dx > 0 ? DIR_RIGHT : DIR_LEFT)
		                                  : (dy > 0 ? DIR_DOWN  : DIR_UP);
		self->fps = 9.0f;
	} else {
		self->fps = 0.0f; self->frame = 0;
	}
	self->row = facing;

	if (dx != 0 && dy != 0) { dx *= 0.7071f; dy *= 0.7071f; }   /* keep diagonal speed even */
	eng_move_and_collide(self, dx * SPD * dt, dy * SPD * dt);
	camera_follow(self->x, self->y);

	/* sword: kill overlapping slimes for its lifetime, then expire */
	if (attack_cd > 0) attack_cd -= dt;
	if (sword) {
		eng_node *e;
		while ((e = eng_overlap_next(sword, T_SLIME, NULL))) eng_node_free(e);
		if ((sword_t -= dt) <= 0) { eng_node_free(sword); sword = NULL; }
	}
	if (eng_just_pressed(ENG_A) && attack_cd <= 0 && !sword) { swing_sword(); attack_cd = SWORD_CD; }

	/* i-frame blink, then contact damage from any overlapping slime */
	if (inv > 0) { inv -= dt; self->tint = (fmodf(inv, 0.2f) < 0.1f) ? ENG_RGB(255,120,120) : ENG_WHITE; }
	else {
		self->tint = ENG_WHITE;
		if (eng_overlap_next(self, T_SLIME, NULL)) hurt_player();
	}
	if (st != S_PLAY) return;   /* a fatal contact hit ends the run — don't let the chest below flip it to WIN */

	/* pickups + the locked goal */
	for (eng_node *h = eng_overlap_next(self, T_HEART, NULL); h; h = eng_overlap_next(self, T_HEART, h))
		{ if (hp < MAXHP) hp++; eng_node_free(h); }
	for (eng_node *k = eng_overlap_next(self, T_KEY, NULL); k; k = eng_overlap_next(self, T_KEY, k))
		{ keys++; eng_node_free(k); }
	if (eng_overlap_next(self, T_CHEST, NULL)) { if (keys > 0) st = S_WIN; else hint_t = 1.2f; }
}

static void slime_update(eng_node *self, float dt)
{
	if (st != S_PLAY || !player) return;
	float dx = player->x - self->x, dy = player->y - self->y;
	float d2 = dx * dx + dy * dy;
	if (d2 > 4.0f && d2 < CHASE_R * CHASE_R) {           /* chase when near, idle-bob otherwise */
		float d = sqrtf(d2);
		eng_move_and_collide(self, dx / d * SLIME_SPD * dt, dy / d * SLIME_SPD * dt);
	}
}

static void spawn(char kind, float x, float y)
{
	eng_node *n;
	switch (kind) {
	case 'p':
		player = n = eng_anim_new(eng_layer(ENG_LAYER_WORLD), img_hero, 32, 32, 4, 0.0f);
		n->tag = T_PLAYER; n->update = player_update; n->z = 10;
		n->w = 20; n->h = 20; facing = DIR_DOWN;
		spawn_x = x; spawn_y = y;
		break;
	case 's':
		n = eng_anim_new(eng_layer(ENG_LAYER_WORLD), img_slime, 32, 32, 2, 4.0f);
		n->tag = T_SLIME; n->update = slime_update; n->z = 6; n->w = 20; n->h = 18;
		break;
	case 'h': n = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_heart); n->tag = T_HEART; n->z = 5; break;
	case 'k': n = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_key);   n->tag = T_KEY;   n->z = 5; break;
	case 'g': n = eng_sprite_new(eng_layer(ENG_LAYER_WORLD), img_chest); n->tag = T_CHEST; n->z = 5;
	          n->w = 28; n->h = 28; break;
	default:  return;
	}
	n->x = x; n->y = y;
}

static void build_level(void)
{
	eng_scene_clear();
	player = sword = NULL;
	hp = 3; keys = 0; attack_cd = inv = sword_t = hint_t = 0;

	eng_tilemap *bgmap = eng_level_bg(level);                           /* ground (non-solid) */
	if (bgmap) { eng_node *b = eng_tilemap_node(eng_layer(ENG_LAYER_WORLD), bgmap); b->z = -2; }
	eng_node *tm = eng_tilemap_node(eng_layer(ENG_LAYER_WORLD), map);   /* obstacles (solid) */
	tm->z = -1;

	int nobj; const eng_object *obj = eng_level_objects(level, &nobj);
	for (int i = 0; i < nobj; i++) spawn(obj[i].name[0], obj[i].x, obj[i].y);

	hud_label = eng_label_new(eng_layer(ENG_LAYER_HUD), 26);           /* screen-fixed HP/keys */
	hud_label->x = 16; hud_label->y = 12; hud_label->text = hud;

	camera_follow(spawn_x, spawn_y);
	st = S_PLAY;
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	img_hero  = eng_image_from_png("adventure/hero.png");
	img_slime = eng_image_from_png("adventure/slime.png");
	img_sword = eng_image_from_png("adventure/sword.png");
	img_heart = eng_image_from_png("adventure/heart.png");
	img_key   = eng_image_from_png("adventure/key.png");
	img_chest = eng_image_from_png("adventure/chest.png");
	level = eng_level_load("adventure/level1.tmj", 32, TILE_KEYS, TILE_COLORS);
	map = eng_level_map(level);
	if (map) { TP = eng_tilemap_px(map);
	           level_w = eng_tilemap_cols(map) * TP; level_h = eng_tilemap_rows(map) * TP; }
	eng_set_tilemap(map);       /* the solid world for eng_move_and_collide */
	st = S_TITLE;
}

static void on_update(float dt)
{
	if (st == S_TITLE) { if (eng_just_pressed(ENG_START)) build_level(); return; }
	if (st == S_WIN || st == S_DEAD) {
		if (eng_just_pressed(ENG_START)) { eng_scene_clear(); player = NULL; st = S_TITLE; }
		return;
	}
	if (hint_t > 0) hint_t -= dt;
	snprintf(hud, sizeof hud, "HP %d/%d   KEYS %d", hp, MAXHP, keys);
}

static void on_draw_background(void)
{
	eng_clear(ENG_RGB(24, 28, 34));   /* only shows at level edges; the ground tilemap covers the rest */
}

static void center(const char *s, int y, int px, eng_color c)
{
	eng_text((W - eng_text_width(px, s)) / 2, y, px, c, s);
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		center("ADVENTURE",   H/2 - 70, 84, ENG_RGB(235, 225, 180));
		center("PRESS START", H/2 + 30, 36, ENG_RGB(220, 220, 230));
	} else if (st == S_WIN) {
		center("YOU ESCAPED!", H/2 - 60, 80, ENG_RGB(240, 220, 90));
		center("PRESS START",  H/2 + 40, 32, ENG_WHITE);
	} else if (st == S_DEAD) {
		center("GAME OVER",   H/2 - 60, 80, ENG_RGB(220, 70, 80));
		center("PRESS START", H/2 + 40, 32, ENG_RGB(230, 230, 240));
	} else if (hint_t > 0) {
		center("NEED A KEY!", H - 70, 34, ENG_RGB(240, 220, 90));
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Adventure", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
