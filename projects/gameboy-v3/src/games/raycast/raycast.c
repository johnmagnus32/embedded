/* games/raycast/raycast.c — a Wolfenstein-style first-person shooter: the game that adds the
 * engine's first 3D. It's a RAYCASTER — for each screen column we march a ray through a grid map
 * (DDA) until it hits a wall, then draw a textured vertical strip scaled by distance (the new
 * eng_tex_column). Enemies are BILLBOARD sprites, projected + depth-tested against a per-column
 * z-buffer so walls occlude them. No GPU, no polygons — pure CPU columns, exactly the software
 * pseudo-3D the T113 is suited to. Contrast with the 2D games: this is a 2.5D renderer over the
 * same tile grid.
 *
 * Controls: UP/DOWN move, LEFT/RIGHT turn, A (z/Space) shoot, START to play / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define PI   3.14159265f
#define MW   16
#define MH   16
#define MAXW 1024
#define MAXEN 8
#define TEXW 64
#define IABS(v) ((v) < 0 ? -(v) : (v))

enum { S_TITLE, S_PLAY, S_WIN, S_DEAD };

#define MOVE      3.0f     /* cells/sec */
#define ROT       2.2f     /* rad/sec */
#define FIRE_CD   0.40f
#define EN_SPD    0.95f    /* enemy chase speed */
#define EN_RANGE  9.0f     /* only chase within this many cells */
#define HEALTH0   100
#define CONTACT_DPS 28.0f  /* damage/sec while an enemy touches you */

/* palette */
#define C_CEIL   ENG_RGB(40, 46, 60)
#define C_FLOOR  ENG_RGB(70, 60, 50)
#define C_HUD    ENG_RGB(150, 220, 255)
#define C_MUTE   ENG_RGB(160, 170, 185)
#define C_HP     ENG_RGB(90, 220, 110)
#define C_WARN   ENG_RGB(240, 90, 90)
#define C_FLASH  ENG_RGB(255, 240, 160)
#define C_CROSS  ENG_RGB(230, 235, 245)
#define C_WIN    ENG_RGB(120, 220, 140)

static const char *MAP[MH] = {
	"1111111111111111",
	"1..............1",
	"1..2.......2...1",
	"1..............1",
	"1....3333......1",
	"1..............1",
	"1.......2......1",
	"1..............1",
	"1..2.......1...1",
	"1..........1...1",
	"1..............1",
	"1....2.2.......1",
	"1..............1",
	"1.........33...1",
	"1..............1",
	"1111111111111111",
};

typedef struct { float x, y; int alive; } Enemy;

static int    W, H, st, kills, ntot;
static float  posX, posY, dirX, dirY, planeX, planeY;
static float  health, fire_cd, flash;
static float  zbuf[MAXW];
static Enemy  en[MAXEN];
static int    nen;
static eng_image *tex_walls, *tex_guard, *tex_gun;

static int solid(int cx, int cy)
{
	if (cx < 0 || cx >= MW || cy < 0 || cy >= MH) return 1;
	return MAP[cy][cx] != '.';
}

static void reset_game(void)
{
	posX = 2.5f; posY = 2.5f; dirX = 1.0f; dirY = 0.0f; planeX = 0.0f; planeY = 0.66f;
	health = HEALTH0; fire_cd = 0; flash = 0; kills = 0;
	for (int i = 0; i < MAXW; i++) zbuf[i] = 1e30f;
	nen = 0;
	float spots[][2] = { {7.5f, 7.5f}, {12.5f, 3.5f}, {4.5f, 12.5f}, {13.5f, 10.5f} };
	for (int i = 0; i < 4; i++) { en[nen].x = spots[i][0]; en[nen].y = spots[i][1]; en[nen].alive = 1; nen++; }
	ntot = nen;
	st = S_PLAY;
}

static void rotate(float a)
{
	float c = cosf(a), s = sinf(a), t;
	t = dirX;   dirX   = dirX * c - dirY * s;     dirY   = t * s + dirY * c;
	t = planeX; planeX = planeX * c - planeY * s; planeY = t * s + planeY * c;
}

static void try_move(float dx, float dy)
{
	float nx = posX + dx, ny = posY + dy;
	if (!solid((int)nx, (int)posY)) posX = nx;
	if (!solid((int)posX, (int)ny)) posY = ny;
}

/* camera-space depth + screen column of an enemy (Lodev sprite transform) */
static void sprite_xform(const Enemy *e, float *depth, int *screenX)
{
	float sx = e->x - posX, sy = e->y - posY;
	float invDet = 1.0f / (planeX * dirY - dirX * planeY);
	float tX = invDet * (dirY * sx - dirX * sy);
	float tY = invDet * (-planeY * sx + planeX * sy);   /* depth */
	*depth = tY;
	*screenX = (tY > 0.001f) ? (int)((W / 2) * (1.0f + tX / tY)) : -9999;
}

static void fire(void)
{
	int best = -1; float bestd = 1e30f;
	for (int i = 0; i < nen; i++) {
		if (!en[i].alive) continue;
		float d; int scr;
		sprite_xform(&en[i], &d, &scr);
		if (d <= 0.15f) continue;
		int halfw = (int)fabsf(H / d) / 2;                  /* on-screen half width */
		if (IABS(scr - W / 2) > halfw) continue;            /* not under the crosshair */
		if (d >= zbuf[W / 2]) continue;                     /* a wall is in the way */
		if (d < bestd) { bestd = d; best = i; }
	}
	if (best >= 0) { en[best].alive = 0; kills++; }
}

static void on_update(float dt)
{
	if (st != S_PLAY) {
		if (eng_just_pressed(ENG_START)) { if (st == S_TITLE) reset_game(); else st = S_TITLE; }
		return;
	}
	if (eng_pressed(ENG_UP))    try_move(dirX * MOVE * dt, dirY * MOVE * dt);
	if (eng_pressed(ENG_DOWN))  try_move(-dirX * MOVE * dt, -dirY * MOVE * dt);
	if (eng_pressed(ENG_LEFT))  rotate(-ROT * dt);   /* turn left */
	if (eng_pressed(ENG_RIGHT)) rotate(ROT * dt);    /* turn right */

	fire_cd -= dt; if (flash > 0) flash -= dt;
	if (eng_pressed(ENG_A) && fire_cd <= 0) { fire(); fire_cd = FIRE_CD; flash = 0.07f; }

	int alive = 0;
	for (int i = 0; i < nen; i++) {
		if (!en[i].alive) continue;
		alive++;
		float dx = posX - en[i].x, dy = posY - en[i].y, d = sqrtf(dx * dx + dy * dy);
		if (d > 0.5f && d < EN_RANGE) {                     /* chase (grid-collided) */
			float sx = dx / d * EN_SPD * dt, sy = dy / d * EN_SPD * dt;
			if (!solid((int)(en[i].x + sx), (int)en[i].y)) en[i].x += sx;
			if (!solid((int)en[i].x, (int)(en[i].y + sy))) en[i].y += sy;
		}
		if (d < 0.55f) health -= CONTACT_DPS * dt;          /* touching -> take damage */
	}
	if (health <= 0) { health = 0; st = S_DEAD; }
	else if (alive == 0) st = S_WIN;
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	tex_walls = eng_image_from_png("raycast/walls.png");
	tex_guard = eng_image_from_png("raycast/guard.png");
	tex_gun   = eng_image_from_png("raycast/gun.png");
	reset_game(); st = S_TITLE;
}

static void cast_walls(void)
{
	for (int x = 0; x < W; x++) {
		float cam = 2.0f * x / W - 1.0f;
		float rdx = dirX + planeX * cam, rdy = dirY + planeY * cam;
		int mapX = (int)posX, mapY = (int)posY;
		float ddx = (rdx == 0) ? 1e30f : fabsf(1.0f / rdx);
		float ddy = (rdy == 0) ? 1e30f : fabsf(1.0f / rdy);
		int stepX, stepY; float sdx, sdy;
		if (rdx < 0) { stepX = -1; sdx = (posX - mapX) * ddx; } else { stepX = 1; sdx = (mapX + 1 - posX) * ddx; }
		if (rdy < 0) { stepY = -1; sdy = (posY - mapY) * ddy; } else { stepY = 1; sdy = (mapY + 1 - posY) * ddy; }
		int side = 0; char cell = '1';
		for (int guard = 0; guard < 64; guard++) {              /* DDA march */
			if (sdx < sdy) { sdx += ddx; mapX += stepX; side = 0; }
			else           { sdy += ddy; mapY += stepY; side = 1; }
			if (mapX < 0 || mapX >= MW || mapY < 0 || mapY >= MH) { cell = '1'; break; }
			if (MAP[mapY][mapX] != '.') { cell = MAP[mapY][mapX]; break; }
		}
		float perp = (side == 0) ? (sdx - ddx) : (sdy - ddy);
		if (perp < 0.01f) perp = 0.01f;
		if (x < MAXW) zbuf[x] = perp;
		int lh = (int)(H / perp);
		int y0 = H / 2 - lh / 2, y1 = H / 2 + lh / 2;
		float wallX = (side == 0) ? posY + perp * rdy : posX + perp * rdx;
		wallX -= floorf(wallX);
		int tx = (int)(wallX * TEXW);
		if ((side == 0 && rdx > 0) || (side == 1 && rdy < 0)) tx = TEXW - 1 - tx;   /* orient */
		int atlasX = (cell - '1') * TEXW + tx;
		int shade = 256 - (int)(perp * 22); if (shade < 70) shade = 70;             /* distance fog */
		if (side == 1) shade = shade * 3 / 4;                                        /* side shading */
		eng_tex_column(tex_walls, x, atlasX, y0, y1, shade, 0);
	}
}

static void cast_sprites(void)
{
	int order[MAXEN], n = 0;
	float dist[MAXEN];
	for (int i = 0; i < nen; i++) {
		if (!en[i].alive) continue;
		float dx = en[i].x - posX, dy = en[i].y - posY;
		order[n] = i; dist[n] = dx * dx + dy * dy; n++;
	}
	for (int a = 1; a < n; a++) {                                /* insertion sort: far -> near */
		int oi = order[a]; float od = dist[a]; int b = a - 1;
		while (b >= 0 && dist[b] < od) { order[b + 1] = order[b]; dist[b + 1] = dist[b]; b--; }
		order[b + 1] = oi; dist[b + 1] = od;
	}
	for (int k = 0; k < n; k++) {
		Enemy *e = &en[order[k]];
		float d; int scr;
		sprite_xform(e, &d, &scr);
		if (d <= 0.15f) continue;
		int sh = (int)fabsf(H / d);
		int y0 = H / 2 - sh / 2, y1 = H / 2 + sh / 2;
		int x0 = scr - sh / 2, x1 = scr + sh / 2;
		int shade = 256 - (int)(d * 22); if (shade < 80) shade = 80;
		for (int x = x0; x < x1; x++) {
			if (x < 0 || x >= W) continue;
			if (x < MAXW && d >= zbuf[x]) continue;             /* occluded by a nearer wall */
			int texX = (sh > 0) ? (int)((long)(x - x0) * TEXW / sh) : 0;
			eng_tex_column(tex_guard, x, texX, y0, y1, shade, 1);
		}
	}
}

static void on_draw_background(void)
{
	eng_rect_fill(0, 0, W, H / 2, C_CEIL);
	eng_rect_fill(0, H / 2, W, H - H / 2, C_FLOOR);
	if (st == S_PLAY || st == S_WIN || st == S_DEAD) { cast_walls(); cast_sprites(); }
}

static void on_draw_overlay(void)
{
	char buf[48];
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 70, 76, C_HUD, ENG_ALIGN_CENTER, "RAYCASTER");
		eng_text_aligned(W / 2, H / 2 + 16, 26, C_MUTE, ENG_ALIGN_CENTER, "ARROWS MOVE/TURN   A SHOOT");
		eng_text_aligned(W / 2, H / 2 + 56, 30, C_HP, ENG_ALIGN_CENTER, "PRESS START");
		return;
	}
	/* crosshair */
	eng_rect_fill(W / 2 - 8, H / 2 - 1, 16, 2, C_CROSS);
	eng_rect_fill(W / 2 - 1, H / 2 - 8, 2, 16, C_CROSS);
	/* gun bottom-center, drawn as full-height (1:1) texture columns; muzzle flash while firing */
	if (tex_gun) {
		int gw = eng_image_w(tex_gun), gh = eng_image_h(tex_gun);
		int gx0 = W / 2 - gw / 2, gy0 = H - gh;
		for (int x = 0; x < gw; x++)
			eng_tex_column(tex_gun, gx0 + x, x, gy0, gy0 + gh, 256, 1);
		if (flash > 0) eng_circle_fill(W / 2, gy0 + 4, 20, C_FLASH);
	}
	/* HUD */
	snprintf(buf, sizeof buf, "HP %d", (int)health);
	eng_text(16, 12, 28, health > 30 ? C_HP : C_WARN, buf);
	snprintf(buf, sizeof buf, "KILLS %d/%d", kills, ntot);
	eng_text_aligned(W - 16, 12, 28, C_HUD, ENG_ALIGN_RIGHT, buf);

	if (st == S_WIN) {
		eng_text_aligned(W / 2, H / 2 - 40, 78, C_WIN, ENG_ALIGN_CENTER, "CLEARED!");
		eng_text_aligned(W / 2, H / 2 + 34, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_DEAD) {
		eng_text_aligned(W / 2, H / 2 - 40, 78, C_WARN, ENG_ALIGN_CENTER, "YOU DIED");
		eng_text_aligned(W / 2, H / 2 + 34, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Raycaster", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
