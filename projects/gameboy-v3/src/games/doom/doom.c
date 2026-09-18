/* games/doom/doom.c — "Doom-lite": a SECTOR / PORTAL software renderer, the step past the
 * raycaster. Where raycast.c marches a grid of same-height walls, this renders a world of
 * hand-built CONVEX SECTORS, each with its own floor and ceiling HEIGHT, connected by PORTALS
 * (two-sided edges you see and walk through). That buys the three things a raycaster can't do:
 *   - variable floor/ceiling heights + steps between rooms (the defining Doom look),
 *   - TEXTURED floors and ceilings (perspective floor-casting, per column) — not flat colors,
 *   - arbitrary (non-grid) wall angles.
 * Algorithm (Bisqwit-style, original code): keep a queue of {sector, screen x-range}. For each
 * sector, transform its edges into camera space, near-clip, project to screen, and per column
 * draw the ceiling plane, floor plane, and wall — maintaining a per-column clip window
 * [ytop,ybot]. A solid edge closes the column; a portal draws the upper/lower step walls,
 * narrows the window to the opening, and enqueues the neighbor sector to fill it. Enemies are
 * floor-aligned billboards, depth-tested against a per-column wall-depth buffer (like raycast).
 *
 * Two new engine primitives do the pixel work: eng_wall_column (clipped textured wall span) and
 * eng_floor_column (perspective plane span). See engine.h.
 *
 * Controls: UP/DOWN move, LEFT/RIGHT turn, A (z/Space) shoot, START to play / restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define PI       3.14159265f
#define MAXW     1024
#define MAXPT    8
#define NSECT    5
#define MAXEN    8
#define MAXQ     64
#define MAXREND  6          /* cap re-renders of one sector per frame (loop guard) */
#define TW       64         /* wall/plane texel tile size */
#define TPU      28.0f      /* wall texels per world unit (tiling density) */
#define PSCALE   0.02f      /* plane texels/world-unit for eng_floor_column (floor tiling) */
#define NEAR     0.05f      /* near clip plane (camera depth) */
#define EYE_H    0.62f      /* eye above the current sector floor */
#define STEP_TILE 2         /* atlas tile used for portal step walls */
#define IABS(v)  ((v) < 0 ? -(v) : (v))

enum { S_TITLE, S_PLAY, S_WIN, S_DEAD };

#define MOVE      2.6f
#define ROT       2.3f
#define PLAYER_R  0.24f
#define FIRE_CD   0.55f
#define EN_SPD    1.05f
#define EN_RANGE  12.0f
#define HEALTH0   100
#define CONTACT_DPS 30.0f

/* palette */
#define C_HUD    ENG_RGB(150, 220, 255)
#define C_MUTE   ENG_RGB(160, 170, 185)
#define C_HP     ENG_RGB(90, 220, 110)
#define C_WARN   ENG_RGB(240, 90, 90)
#define C_FLASH  ENG_RGB(255, 240, 170)
#define C_CROSS  ENG_RGB(230, 235, 245)
#define C_WIN    ENG_RGB(120, 220, 140)

/* A convex sector: a loop of `npts` vertices (px,py), with neigh[i] = the sector reachable across
 * edge i (pts[i]->pts[i+1]), or -1 for a solid wall. floor/ceil are world heights; wtex indexes
 * the wall atlas tile. All sectors wound the same way (verified against the back-face test). */
typedef struct {
	int   npts;
	float px[MAXPT], py[MAXPT];
	int   neigh[MAXPT];
	float floor, ceil;
	int   wtex;
} Sector;

/* Hand-built level: start room -> low corridor -> far room with a RAISED floor, plus a side
 * alcove off the start room with a LOWERED floor (a pit). Shared portal edges are listed in
 * opposite directions in the two sectors (as a shared edge must be). */
static Sector sect[NSECT] = {
	/* 0: start room 0..6 x 0..6, floor 0, ceil 3.0. Right edge split for the corridor portal;
	 *    bottom edge split for the alcove portal. */
	{ 6, {0,6,6,6,6,0},   {0,0,2,4,6,6},   {-1,-1, 1,-1,-1,-1}, 0.00f, 3.00f, 0 },
	/* 1: corridor 6..10 x 2..4, floor 0, low ceil 2.3. Left portal->0, right portal->2. */
	{ 4, {6,10,10,6},     {2,2,4,4},       { -1, 2,-1, 0},      0.00f, 2.30f, 3 },
	/* 2: far room 10..16 x 0..6, RAISED floor 0.55, tall ceil 3.6. Left edge split for the portal. */
	{ 6, {10,16,16,10,10,10}, {0,0,6,6,4,2}, {-1,-1,-1,-1, 1,-1}, 0.55f, 3.60f, 1 },
	/* 3: pit alcove 1.5..4.5 x -3..0, LOWERED floor -0.5, ceil 2.6. Top edge portal->0. */
	{ 4, {1.5f,4.5f,4.5f,1.5f}, {-3,-3,0,0}, {-1,-1, -1, -1}, -0.50f, 2.60f, 2 },
	/* 4: unused spare (kept so NSECT math is simple) */
	{ 0, {0}, {0}, {0}, 0, 0, 0 },
};

typedef struct { float x, y, floorz; int alive; } Enemy;

static int    W, H, st, kills, ntot, cur;
static float  posX, posY, ang, eyeZ;
static float  health, fire_cd, flash, hurt_cd;
static int    ytop[MAXW], ybot[MAXW];
static float  zwall[MAXW];
static Enemy  en[MAXEN];
static int    nen;
static eng_image *tex_walls, *tex_floor, *tex_ceil, *tex_imp, *tex_gun;
static eng_sound *sfx_shot, *sfx_die, *sfx_hurt;

/* wire the alcove portal on sector 0's bottom edge (edge index 1: pts[1]->pts[2] is (6,0)->(6,2),
 * so the alcove is NOT there) — instead the alcove hangs off a dedicated split; keep it simple:
 * connect sector 3's top edge to sector 0 and add the matching opening on sector 0's left-bottom.
 * (Done in level_link so the static table stays readable.) */
static void level_link(void)
{
	/* Sector 0 gets an extra portal to the pit (sector 3) by replacing its bottom wall with a
	 * split: rebuild sector 0 as an 8-gon with the opening (1.5..4.5) on the bottom edge. */
	Sector s0 = { 8,
		{0, 1.5f, 4.5f, 6, 6, 6, 6, 0},
		{0, 0,    0,    0, 2, 4, 6, 6},
		{-1, 3, -1, -1, 1, -1, -1, -1},   /* edge1 (1.5,0)->(4.5,0) -> pit; edge4 (6,2)->(6,4) -> corridor */
		0.00f, 3.00f, 0 };
	sect[0] = s0;
	/* Sector 3's top edge (4.5,0)->(1.5,0) faces sector 0. */
	sect[3].neigh[2] = 0;   /* pts[2]->pts[3] = (4.5,0)->(1.5,0) */
}

static int point_in_sector(int si, float x, float y)
{
	Sector *s = &sect[si];
	if (s->npts < 3) return 0;
	int pos = 0, neg = 0;
	for (int i = 0; i < s->npts; i++) {
		int j = (i + 1) % s->npts;
		float ex = s->px[j] - s->px[i], ey = s->py[j] - s->py[i];
		float cr = ex * (y - s->py[i]) - ey * (x - s->px[i]);
		if (cr > 0.0f) pos = 1; else if (cr < 0.0f) neg = 1;
	}
	return !(pos && neg);   /* inside a convex poly: all cross products share one sign */
}

static int find_sector(float x, float y, int guess)
{
	if (guess >= 0 && guess < NSECT && point_in_sector(guess, x, y)) return guess;
	for (int i = 0; i < NSECT; i++) if (point_in_sector(i, x, y)) return i;
	return guess;   /* keep the last known if we somehow left the map */
}

static void reset_game(void)
{
	posX = 3.0f; posY = 3.0f; ang = 0.0f;
	cur = find_sector(posX, posY, 0);
	eyeZ = sect[cur].floor + EYE_H;
	health = HEALTH0; fire_cd = 0; flash = 0; hurt_cd = 0; kills = 0;
	nen = 0;
	float spots[][2] = { {13.0f, 3.0f}, {14.0f, 1.5f}, {12.0f, 4.6f}, {3.0f, -1.5f} };
	for (int i = 0; i < 4; i++) {
		en[nen].x = spots[i][0]; en[nen].y = spots[i][1]; en[nen].alive = 1;
		int es = find_sector(en[nen].x, en[nen].y, -1);
		en[nen].floorz = (es >= 0 && es < NSECT) ? sect[es].floor : 0.0f;
		nen++;
	}
	ntot = nen;
	st = S_PLAY;
}

/* slide the player circle out of the current sector's SOLID edges (portals pass through) */
static void collide(void)
{
	for (int pass = 0; pass < 3; pass++) {
		Sector *s = &sect[cur];
		int hit = 0;
		for (int i = 0; i < s->npts; i++) {
			if (s->neigh[i] >= 0) continue;   /* portal: not solid */
			int j = (i + 1) % s->npts;
			float nx, ny, pen;
			if (eng_circle_segment(posX, posY, PLAYER_R, s->px[i], s->py[i], s->px[j], s->py[j],
			                       &nx, &ny, &pen)) {
				posX += nx * pen; posY += ny * pen; hit = 1;
			}
		}
		if (!hit) break;
	}
}

static void on_update(float dt)
{
	if (st != S_PLAY) {
		if (eng_just_pressed(ENG_START)) { if (st == S_TITLE) reset_game(); else st = S_TITLE; }
		return;
	}
	if (eng_pressed(ENG_LEFT))  ang += ROT * dt;   /* turn view left (world sweeps right) */
	if (eng_pressed(ENG_RIGHT)) ang -= ROT * dt;
	float ca = cosf(ang), sa = sinf(ang), mv = 0.0f;
	if (eng_pressed(ENG_UP))   mv += MOVE * dt;
	if (eng_pressed(ENG_DOWN)) mv -= MOVE * dt;
	posX += ca * mv; posY += sa * mv;
	collide();
	cur = find_sector(posX, posY, cur);
	eyeZ = sect[cur].floor + EYE_H;

	fire_cd -= dt; if (flash > 0) flash -= dt; if (hurt_cd > 0) hurt_cd -= dt;
	if (eng_pressed(ENG_A) && fire_cd <= 0) {
		/* hitscan straight down the crosshair: nearest alive enemy near screen center, unoccluded */
		int best = -1; float bestd = 1e30f;
		for (int i = 0; i < nen; i++) {
			if (!en[i].alive) continue;
			float dx = en[i].x - posX, dy = en[i].y - posY;
			float d = dx * ca + dy * sa, r = dx * sa - dy * ca;
			if (d <= 0.3f) continue;
			if (IABS(r) > 0.45f * d) continue;                 /* outside ~crosshair cone */
			if (W / 2 < MAXW && d >= zwall[W / 2]) continue;   /* wall in the way */
			if (d < bestd) { bestd = d; best = i; }
		}
		if (best >= 0) { en[best].alive = 0; kills++; if (sfx_die) eng_sound_play(sfx_die, 0.7f); }
		if (sfx_shot) eng_sound_play(sfx_shot, 0.6f);
		fire_cd = FIRE_CD; flash = 0.08f;
	}

	int alive = 0;
	for (int i = 0; i < nen; i++) {
		if (!en[i].alive) continue;
		alive++;
		float dx = posX - en[i].x, dy = posY - en[i].y, d = sqrtf(dx * dx + dy * dy);
		if (d > 0.5f && d < EN_RANGE) {                        /* chase toward the player */
			en[i].x += dx / d * EN_SPD * dt;
			en[i].y += dy / d * EN_SPD * dt;
			int es = find_sector(en[i].x, en[i].y, -1);
			if (es >= 0 && es < NSECT) en[i].floorz = sect[es].floor;
		}
		if (d < 0.6f) {                                        /* touching: take damage */
			health -= CONTACT_DPS * dt;
			if (hurt_cd <= 0 && sfx_hurt) { eng_sound_play(sfx_hurt, 0.5f); hurt_cd = 0.35f; }
		}
	}
	if (health <= 0) { health = 0; st = S_DEAD; }
	else if (alive == 0) st = S_WIN;
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	level_link();
	tex_walls = eng_image_from_png("doom/walls.png");
	tex_floor = eng_image_from_png("doom/floor.png");
	tex_ceil  = eng_image_from_png("doom/ceil.png");
	tex_imp   = eng_image_from_png("doom/imp.png");
	tex_gun   = eng_image_from_png("doom/gun.png");
	sfx_shot  = eng_sound_load("sfx/shotgun.wav");
	sfx_die   = eng_sound_load("sfx/growl.wav");
	sfx_hurt  = eng_sound_load("sfx/hurt.wav");
	reset_game(); st = S_TITLE;
}

static int shade_of(float d) { int s = 256 - (int)(d * 24.0f); return s < 70 ? 70 : s; }

/* project a world point (relative already transformed) — helpers inlined in render for speed */
static void render_world(void)
{
	float ca = cosf(ang), sa = sinf(ang);
	int horizon = H / 2;
	float VS = (float)H, XS = (float)H;
	for (int x = 0; x < W; x++) { ytop[x] = 0; ybot[x] = H - 1; if (x < MAXW) zwall[x] = 1e30f; }

	struct { int sect, x1, x2; } q[MAXQ];
	int qh = 0, qt = 0, rendered[NSECT];
	for (int i = 0; i < NSECT; i++) rendered[i] = 0;
	q[qt].sect = cur; q[qt].x1 = 0; q[qt].x2 = W - 1; qt++;

	while (qh < qt) {
		int si = q[qh].sect, qx1 = q[qh].x1, qx2 = q[qh].x2; qh++;
		if (si < 0 || si >= NSECT) continue;
		if (rendered[si]++ >= MAXREND) continue;
		Sector *s = &sect[si];

		for (int e = 0; e < s->npts; e++) {
			int e2 = (e + 1) % s->npts;
			float ax = s->px[e]  - posX, ay = s->py[e]  - posY;
			float bx = s->px[e2] - posX, by = s->py[e2] - posY;
			float d1 = ax * ca + ay * sa, r1 = ax * sa - ay * ca;   /* camera: depth, right */
			float d2 = bx * ca + by * sa, r2 = bx * sa - by * ca;
			float u1 = 0.0f, u2 = 1.0f;                             /* param along the wall */
			if (d1 < NEAR && d2 < NEAR) continue;
			if (d1 < NEAR) { float t = (NEAR - d1) / (d2 - d1); r1 += t * (r2 - r1); u1 += t * (u2 - u1); d1 = NEAR; }
			else if (d2 < NEAR) { float t = (NEAR - d2) / (d1 - d2); r2 += t * (r1 - r2); u2 += t * (u1 - u2); d2 = NEAR; }

			int sx1 = (int)(W / 2 + r1 / d1 * XS);
			int sx2 = (int)(W / 2 + r2 / d2 * XS);
			/* Sectors wind CCW (interior on each edge's left), so a wall FACING the camera runs
			 * right->left on screen (sx1 > sx2). Cull back-faces; then swap so index 1 = the LEFT
			 * column and the rest of the code scans left->right. */
			if (sx1 <= sx2) continue;
			{ float t; t = d1; d1 = d2; d2 = t; t = u1; u1 = u2; u2 = t; int is = sx1; sx1 = sx2; sx2 = is; }
			int xL = sx1 < qx1 ? qx1 : sx1, xR = sx2 > qx2 ? qx2 : sx2;
			if (xL < 0) xL = 0;
			if (xR > W - 1) xR = W - 1;
			if (xL > xR) continue;

			/* screen y of this sector's ceil/floor at both endpoints (a 3D line -> a 2D line) */
			float yc1 = horizon - (s->ceil  - eyeZ) * VS / d1, yc2 = horizon - (s->ceil  - eyeZ) * VS / d2;
			float yf1 = horizon - (s->floor - eyeZ) * VS / d1, yf2 = horizon - (s->floor - eyeZ) * VS / d2;
			Sector *nb = (s->neigh[e] >= 0) ? &sect[s->neigh[e]] : NULL;
			float ync1 = 0, ync2 = 0, ynf1 = 0, ynf2 = 0;
			if (nb) {
				ync1 = horizon - (nb->ceil  - eyeZ) * VS / d1; ync2 = horizon - (nb->ceil  - eyeZ) * VS / d2;
				ynf1 = horizon - (nb->floor - eyeZ) * VS / d1; ynf2 = horizon - (nb->floor - eyeZ) * VS / d2;
			}
			/* wall length (full, unclipped) for texture tiling; perspective-correct u via u/z, 1/z */
			float wdx = s->px[e2] - s->px[e], wdy = s->py[e2] - s->py[e];
			float wlen = sqrtf(wdx * wdx + wdy * wdy);
			float uoz1 = u1 / d1, uoz2 = u2 / d2, oz1 = 1.0f / d1, oz2 = 1.0f / d2;
			float invspan = 1.0f / (float)(sx2 - sx1);

			for (int x = xL; x <= xR; x++) {
				if (ytop[x] > ybot[x]) continue;             /* column already closed */
				float t = (x - sx1) * invspan;
				int cy = (int)(yc1 + (yc2 - yc1) * t);
				int fy = (int)(yf1 + (yf2 - yf1) * t);
				int cyc = cy < ytop[x] ? ytop[x] : (cy > ybot[x] + 1 ? ybot[x] + 1 : cy);
				int fyc = fy < ytop[x] ? ytop[x] : (fy > ybot[x] + 1 ? ybot[x] + 1 : fy);

				float depth = 1.0f / (oz1 + (oz2 - oz1) * t);
				float camx = (x - W / 2) / XS;
				float rdx = ca + camx * sa, rdy = sa - camx * ca;   /* this column's world ray dir */

				/* ceiling plane above the wall */
				if (cyc > ytop[x] && s->ceil > eyeZ)
					eng_floor_column(tex_ceil, x, ytop[x], cyc, horizon,
					                 (s->ceil - eyeZ) * VS, rdx, rdy, posX, posY, PSCALE, 20);
				/* floor plane below the wall */
				if (fyc <= ybot[x] && s->floor < eyeZ)
					eng_floor_column(tex_floor, x, fyc, ybot[x] + 1, horizon,
					                 (eyeZ - s->floor) * VS, rdx, rdy, posX, posY, PSCALE, 20);

				float uf = (uoz1 + (uoz2 - uoz1) * t) * depth;      /* perspective u in [0,1] */
				int localcol = ((int)(uf * wlen * TPU)) & (TW - 1);
				int sh = shade_of(depth);

				if (!nb) {                                          /* SOLID wall closes the column */
					eng_wall_column(tex_walls, x, s->wtex * TW + localcol, cy, fy, cyc, fyc, sh);
					if (x < MAXW) zwall[x] = depth;
					ytop[x] = ybot[x] + 1;
				} else {                                            /* PORTAL: steps + see-through */
					int nyc = (int)(ync1 + (ync2 - ync1) * t);
					int nyf = (int)(ynf1 + (ynf2 - ynf1) * t);
					if (nb->ceil < s->ceil) {                       /* upper step wall */
						int a = cyc, b = nyc < ytop[x] ? ytop[x] : (nyc > ybot[x] + 1 ? ybot[x] + 1 : nyc);
						if (b > a) eng_wall_column(tex_walls, x, STEP_TILE * TW + localcol, cy, nyc, a, b, sh);
					}
					if (nb->floor > s->floor) {                     /* lower step wall */
						int a = nyf < ytop[x] ? ytop[x] : (nyf > ybot[x] + 1 ? ybot[x] + 1 : nyf), b = fyc;
						if (b > a) eng_wall_column(tex_walls, x, STEP_TILE * TW + localcol, nyf, fy, a, b, sh);
					}
					int nt = cy, nbo = fy;                          /* narrow window to the opening */
					if (nb->ceil  < s->ceil  && nyc > nt)  nt  = nyc;
					if (nb->floor > s->floor && nyf < nbo) nbo = nyf;
					if (nt > ytop[x]) ytop[x] = nt;
					if (nbo - 1 < ybot[x]) ybot[x] = nbo - 1;
				}
			}
			if (nb && qt < MAXQ && xL <= xR) { q[qt].sect = s->neigh[e]; q[qt].x1 = xL; q[qt].x2 = xR; qt++; }
		}
	}
}

/* floor-aligned billboard enemies, far->near, depth-tested against zwall (like the raycaster) */
static void render_enemies(void)
{
	if (!tex_imp) return;
	float ca = cosf(ang), sa = sinf(ang), XS = (float)H, VS = (float)H;
	int horizon = H / 2;
	int order[MAXEN], n = 0; float dsq[MAXEN];
	for (int i = 0; i < nen; i++) {
		if (!en[i].alive) continue;
		float dx = en[i].x - posX, dy = en[i].y - posY;
		order[n] = i; dsq[n] = dx * dx + dy * dy; n++;
	}
	for (int a = 1; a < n; a++) {                     /* insertion sort far->near */
		int oi = order[a]; float od = dsq[a]; int b = a - 1;
		while (b >= 0 && dsq[b] < od) { order[b + 1] = order[b]; dsq[b + 1] = dsq[b]; b--; }
		order[b + 1] = oi; dsq[b + 1] = od;
	}
	int iw = eng_image_w(tex_imp), ih = eng_image_h(tex_imp);
	for (int k = 0; k < n; k++) {
		Enemy *e = &en[order[k]];
		float dx = e->x - posX, dy = e->y - posY;
		float depth = dx * ca + dy * sa, right = dx * sa - dy * ca;
		if (depth <= 0.2f) continue;
		int screenX = (int)(W / 2 + right / depth * XS);
		float worldH = 1.35f;
		int hpx = (int)(worldH * VS / depth);
		int wpx = hpx * iw / ih;
		int yfeet = horizon - (int)((e->floorz - eyeZ) * VS / depth);
		int ytopS = yfeet - hpx;
		int x0 = screenX - wpx / 2, x1 = screenX + wpx / 2;
		int sh = shade_of(depth);
		for (int x = x0; x < x1; x++) {
			if (x < 0 || x >= W) continue;
			if (x < MAXW && depth >= zwall[x]) continue;     /* behind a wall */
			int texX = (wpx > 0) ? (x - x0) * iw / wpx : 0;
			eng_tex_column(tex_imp, x, texX, ytopS, yfeet, sh, 1);
		}
	}
}

static void on_draw_background(void)
{
	eng_clear(ENG_BLACK);
	if (st == S_PLAY || st == S_WIN || st == S_DEAD) { render_world(); render_enemies(); }
}

static void on_draw_overlay(void)
{
	char buf[48];
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 78, 76, C_HUD, ENG_ALIGN_CENTER, "SECTOR ZERO");
		eng_text_aligned(W / 2, H / 2 + 6,  24, C_MUTE, ENG_ALIGN_CENTER, "ARROWS MOVE/TURN   A SHOOT");
		eng_text_aligned(W / 2, H / 2 + 46, 30, C_HP,   ENG_ALIGN_CENTER, "PRESS START");
		return;
	}
	/* crosshair */
	eng_rect_fill(W / 2 - 8, H / 2 - 1, 16, 2, C_CROSS);
	eng_rect_fill(W / 2 - 1, H / 2 - 8, 2, 16, C_CROSS);
	/* gun bottom-center as 1:1 texture columns; muzzle flash while firing */
	if (tex_gun) {
		int gw = eng_image_w(tex_gun), gh = eng_image_h(tex_gun);
		int gx0 = W / 2 - gw / 2, gy0 = H - gh;
		for (int x = 0; x < gw; x++)
			eng_tex_column(tex_gun, gx0 + x, x, gy0, gy0 + gh, 256, 1);
		if (flash > 0) eng_circle_fill(W / 2, gy0 + 6, 22, C_FLASH);
	}
	snprintf(buf, sizeof buf, "HP %d", (int)health);
	eng_text(16, 12, 28, health > 30 ? C_HP : C_WARN, buf);
	snprintf(buf, sizeof buf, "KILLS %d/%d", kills, ntot);
	eng_text_aligned(W - 16, 12, 28, C_HUD, ENG_ALIGN_RIGHT, buf);

	if (st == S_WIN) {
		eng_text_aligned(W / 2, H / 2 - 40, 78, C_WIN,  ENG_ALIGN_CENTER, "CLEARED!");
		eng_text_aligned(W / 2, H / 2 + 34, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_DEAD) {
		eng_text_aligned(W / 2, H / 2 - 40, 78, C_WARN, ENG_ALIGN_CENTER, "YOU DIED");
		eng_text_aligned(W / 2, H / 2 + 34, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Doom-lite", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
