/* games/td/td.c — Balloon TD: a Bloons-style tower defense, TOUCH-DRIVEN. BALLOONS follow a fixed
 * winding PATH (layered — a dart pops one layer and reveals the next color); you buy MONKEY
 * defenders from a shop PANEL on the right and place them on the grass by tapping. Between waves
 * the game PAUSES (state S_READY) so you can build; a START WAVE button (bottom-right) launches the
 * next round. Three defender types trade off cost / range / rate / spread / damage.
 *
 * Screen splits into a 620-wide PLAY field (a baked grass+path texture, td/bg.png) and a 180-wide
 * wood shop PANEL (td/panel.png). Balloons/monkeys are PNG sprites (eng_draw_image, monkeys tinted
 * per type); the path is baked into bg.png so its waypoints MUST match tools/gen_td_assets.py.
 *
 * Controls: MOUSE = finger — tap a panel button to pick a defender, tap the grass to place it, tap
 * START WAVE to begin. Keyboard: START (Enter) also starts the wave / advances title & game-over.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define PI 3.14159265f
enum { S_TITLE, S_READY, S_PLAY, S_WIN, S_DEAD };

/* layout — keep PLAY_W / PATH / PATH_HALF in sync with tools/gen_td_assets.py */
#define PLAY_W     620
#define PANEL_X    620
#define PANEL_W    180
#define PATH_HALF  16

#define BALLOON_R  13
#define MONKEY_R   15
#define DART_R     4
#define DART_SPD   340.0f
#define CASH0      300
#define LIVES0     100
#define MAX_ROUND  10
#define SPAWN_INT  0.60f

#define MAXBAL  80
#define MAXMON  40
#define MAXDART 160
#define MAXPOP  64
#define POP_LIFE 0.18f
#define POP_CELL   40
#define POP_FRAMES 6

/* shop panel geometry */
#define PBTN_X   628
#define PBTN_W   164
#define PBTN_Y0  38
#define PBTN_H   76
#define PBTN_GAP 6
#define START_X  628
#define START_Y  418
#define START_W  164
#define START_H  48

/* palette */
#define C_DART   ENG_RGB(60, 55, 50)
#define C_OK     ENG_RGB(120, 230, 150)
#define C_NO     ENG_RGB(230, 90, 90)
#define C_HUD    ENG_RGB(240, 245, 255)
#define C_MUTE   ENG_RGB(205, 195, 175)
#define C_WIN    ENG_RGB(140, 240, 160)

/* defender types (original — not from any game): name, cost, range, fire cooldown, layers popped
 * per dart, darts per shot, spread (radians) for multi-dart, and the body/icon tint. */
typedef struct {
	const char *name;
	int   cost;
	float range, cd;
	int   dmg, darts;
	float spread;
	eng_color tint;
} MonkeyType;

static const MonkeyType TYPES[] = {
	{ "Slinger", 100, 118.0f, 0.55f, 1, 1, 0.00f, ENG_WHITE },
	{ "Scatter", 175,  96.0f, 0.75f, 1, 3, 0.80f, ENG_RGB(150, 180, 255) },
	{ "Sniper",  260, 320.0f, 1.10f, 2, 1, 0.00f, ENG_RGB(150, 235, 150) },
};
#define NTYPES ((int)(sizeof(TYPES) / sizeof(TYPES[0])))

static const float PATH[][2] = {
	{ -40, 90 }, { 470, 90 }, { 470, 180 }, { 90, 180 },
	{ 90, 290 }, { 500, 290 }, { 500, 390 }, { -40, 390 },
};
#define NWP ((int)(sizeof(PATH) / sizeof(PATH[0])))

/* balloon color by remaining layer (1..4) */
static const eng_color BCOL[5] = {
	0, ENG_RGB(220, 60, 60), ENG_RGB(70, 120, 220), ENG_RGB(70, 190, 90), ENG_RGB(232, 212, 70),
};

typedef struct { float x, y; int seg, layer, alive; } Balloon;
typedef struct { float x, y, cd, aimx, aimy; int type; } Monkey;
typedef struct { float x, y, vx, vy; int alive, dmg; } Dart;
typedef struct { float x, y, t; int alive; } Pop;   /* transient "pow" burst at a dart hit */

static int   W, H, st, cash, lives, round_, to_spawn, sel_type;
static float spawn_t;
static Balloon bal[MAXBAL];
static Monkey  mon[MAXMON]; static int nmon;
static Dart    dart[MAXDART];
static Pop     pops[MAXPOP];
static eng_image *img_balloon, *img_monkey, *img_bg, *img_panel, *img_pop;
static eng_sound *sfx_pop, *sfx_leak, *sfx_place, *sfx_ui, *sfx_start, *mus;

static float spd_for(int layer) { return 48.0f * (1.0f + 0.10f * (layer - 1)); }

static float seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
	float ex = bx - ax, ey = by - ay, l2 = ex * ex + ey * ey, t = 0;
	if (l2 > 1e-6f) { t = ((px - ax) * ex + (py - ay) * ey) / l2; if (t < 0) t = 0; else if (t > 1) t = 1; }
	return hypotf(px - (ax + t * ex), py - (ay + t * ey));
}
static float path_dist(float px, float py)
{
	float m = 1e30f;
	for (int i = 0; i < NWP - 1; i++) {
		float d = seg_dist(px, py, PATH[i][0], PATH[i][1], PATH[i + 1][0], PATH[i + 1][1]);
		if (d < m) m = d;
	}
	return m;
}

/* Where the balloon will be after `t` seconds, walking the ACTUAL path (so leads survive corners). */
static void predict_pos(const Balloon *b, float t, float *ox, float *oy)
{
	float rem = spd_for(b->layer) * t, cx = b->x, cy = b->y;
	int seg = b->seg;
	while (seg < NWP - 1) {
		float dx = PATH[seg + 1][0] - cx, dy = PATH[seg + 1][1] - cy, d = hypotf(dx, dy);
		if (rem <= d || d < 1e-6f) {
			if (d > 1e-6f) { cx += dx / d * rem; cy += dy / d * rem; }
			*ox = cx; *oy = cy; return;
		}
		rem -= d; cx = PATH[seg + 1][0]; cy = PATH[seg + 1][1]; seg++;
	}
	*ox = cx; *oy = cy;                        /* ran off the end (about to leak) */
}

/* Aim direction that INTERCEPTS the balloon: fixed-point solve for dart flight time (predict where
 * it'll be, aim there, recompute the distance, repeat). A few iterations converge. */
static void lead_dir(float mx, float my, const Balloon *b, float *ax, float *ay)
{
	float t = 0, px = b->x, py = b->y;
	for (int k = 0; k < 4; k++) {
		predict_pos(b, t, &px, &py);
		t = hypotf(px - mx, py - my) / DART_SPD;
	}
	float dx = px - mx, dy = py - my, d = hypotf(dx, dy);
	if (d < 1e-3f) d = 1;
	*ax = dx / d; *ay = dy / d;
}

static void spawn_pop(float x, float y)
{
	for (int i = 0; i < MAXPOP; i++)
		if (!pops[i].alive) { pops[i] = (Pop){ x, y, 0, 1 }; return; }
}

static int round_layer(void) { int l = 1 + (round_ - 1) / 2; return l > 4 ? 4 : l; }

static void spawn_balloon(int layer)
{
	for (int i = 0; i < MAXBAL; i++)
		if (!bal[i].alive) { bal[i] = (Balloon){ PATH[0][0], PATH[0][1], 0, layer, 1 }; return; }
}

static void start_wave(void)                       /* leave the pause, unleash the current round */
{
	to_spawn = 5 + 3 * round_;
	spawn_t = 0.4f;
	st = S_PLAY;
	eng_sound_play(sfx_start, 0.7f);
}

static void new_game(void)
{
	for (int i = 0; i < MAXBAL; i++) bal[i].alive = 0;
	for (int i = 0; i < MAXDART; i++) dart[i].alive = 0;
	for (int i = 0; i < MAXPOP; i++) pops[i].alive = 0;
	nmon = 0; cash = CASH0; lives = LIVES0; round_ = 1; sel_type = 0;
	st = S_READY;                                  /* pre-wave pause: build, then START WAVE */
	eng_music_play(mus, 0.35f);                    /* looping bed for the run */
}

static void fire_dart(float x, float y, float vx, float vy, int dmg)
{
	for (int i = 0; i < MAXDART; i++)
		if (!dart[i].alive) { dart[i] = (Dart){ x, y, vx, vy, 1, dmg }; return; }
}

static void pop(Balloon *b, int dmg)               /* remove up to dmg layers, +2 cash each */
{
	while (dmg-- > 0 && b->alive) { cash += 2; if (--b->layer <= 0) b->alive = 0; }
}

static int alive_balloons(void)
{
	int n = 0;
	for (int i = 0; i < MAXBAL; i++) if (bal[i].alive) n++;
	return n;
}

static int can_build(float x, float y, int type)
{
	if (cash < TYPES[type].cost) return 0;
	if (x < MONKEY_R || x > PLAY_W - MONKEY_R || y < MONKEY_R || y > H - MONKEY_R) return 0;
	if (path_dist(x, y) < PATH_HALF + MONKEY_R) return 0;     /* not on the track */
	for (int i = 0; i < nmon; i++)
		if (hypotf(x - mon[i].x, y - mon[i].y) < 2 * MONKEY_R) return 0;   /* not on another monkey */
	return 1;
}

static int type_btn_top(int i)  { return PBTN_Y0 + i * (PBTN_H + PBTN_GAP); }
static int in_rect(int px, int py, int x, int y, int w, int h)
{ return px >= x && px < x + w && py >= y && py < y + h; }

static void place_at(int mx, int my)
{
	if (sel_type < 0) return;                          /* nothing selected -> nothing to place */
	if (can_build(mx, my, sel_type) && nmon < MAXMON) {
		mon[nmon++] = (Monkey){ (float)mx, (float)my, 0, 1, 0, sel_type };
		cash -= TYPES[sel_type].cost;
		eng_sound_play(sfx_place, 0.7f);
	}
}

static void on_tap(int mx, int my)                 /* one pointer/START press */
{
	if (st == S_TITLE)              { new_game(); return; }
	if (st == S_WIN || st == S_DEAD){ st = S_TITLE; return; }
	/* S_READY / S_PLAY */
	if (mx >= PANEL_X) {                            /* the shop panel */
		for (int i = 0; i < NTYPES; i++)
			if (in_rect(mx, my, PBTN_X, type_btn_top(i), PBTN_W, PBTN_H)) {
				sel_type = (sel_type == i) ? -1 : i;   /* tap the selected one again to deselect */
				eng_sound_play(sfx_ui, 0.6f);
				return;
			}
		if (st == S_READY && in_rect(mx, my, START_X, START_Y, START_W, START_H)) start_wave();
	} else {                                        /* the play field */
		place_at(mx, my);
	}
}

static void on_update(float dt)
{
	/* At most ONE input drives a transition per frame — else a same-frame tap+START would chain
	 * TITLE->READY->PLAY and skip the build pause. */
	if (eng_pointer_just_pressed()) { int mx, my; eng_pointer(&mx, &my); on_tap(mx, my); }
	else if (eng_just_pressed(ENG_START)) {         /* keyboard: start wave / advance screens */
		if (st == S_TITLE) new_game();
		else if (st == S_READY) start_wave();
		else if (st == S_WIN || st == S_DEAD) st = S_TITLE;
	}
	for (int i = 0; i < MAXPOP; i++)                /* age the pow bursts (always, so they finish) */
		if (pops[i].alive && (pops[i].t += dt) >= POP_LIFE) pops[i].alive = 0;
	if (st != S_PLAY) return;

	/* spawn the wave */
	if (to_spawn > 0) {
		if ((spawn_t -= dt) <= 0) { spawn_balloon(round_layer()); to_spawn--; spawn_t = SPAWN_INT; }
	} else if (alive_balloons() == 0) {             /* wave cleared -> PAUSE (or win) */
		if (round_ >= MAX_ROUND) { st = S_WIN; return; }
		cash += 40 + round_ * 10;                   /* end-of-wave bonus */
		round_++; st = S_READY;
	}

	/* balloons walk the path */
	for (int i = 0; i < MAXBAL; i++) {
		Balloon *b = &bal[i];
		if (!b->alive) continue;
		float tx = PATH[b->seg + 1][0], ty = PATH[b->seg + 1][1];
		float dx = tx - b->x, dy = ty - b->y, d = hypotf(dx, dy), step = spd_for(b->layer) * dt;
		if (d <= step) {
			b->x = tx; b->y = ty; b->seg++;
			if (b->seg >= NWP - 1) { lives -= b->layer; b->alive = 0; eng_sound_play(sfx_leak, 0.8f); }   /* leaked */
		} else { b->x += dx / d * step; b->y += dy / d * step; }
	}

	/* monkeys fire at the nearest balloon in range (per-type range/rate/spread/damage) */
	for (int m = 0; m < nmon; m++) {
		const MonkeyType *T = &TYPES[mon[m].type];
		if ((mon[m].cd -= dt) > 0) continue;
		int best = -1; float bestd = T->range;
		for (int i = 0; i < MAXBAL; i++) {
			if (!bal[i].alive) continue;
			float dd = hypotf(bal[i].x - mon[m].x, bal[i].y - mon[m].y);
			if (dd < bestd) { bestd = dd; best = i; }
		}
		if (best < 0) continue;
		float ax, ay;
		lead_dir(mon[m].x, mon[m].y, &bal[best], &ax, &ay);   /* aim where it WILL be, not where it is */
		mon[m].aimx = ax; mon[m].aimy = ay;
		float base = atan2f(ay, ax);
		for (int k = 0; k < T->darts; k++) {
			float off = (T->darts > 1) ? T->spread * ((float)k / (T->darts - 1) - 0.5f) : 0.0f;
			float a = base + off;
			fire_dart(mon[m].x, mon[m].y, cosf(a) * DART_SPD, sinf(a) * DART_SPD, T->dmg);
		}
		mon[m].cd = T->cd;
	}

	/* darts fly + pop the first balloon they touch */
	for (int i = 0; i < MAXDART; i++) {
		Dart *p = &dart[i];
		if (!p->alive) continue;
		p->x += p->vx * dt; p->y += p->vy * dt;
		if (p->x < -10 || p->x > PLAY_W + 10 || p->y < -10 || p->y > H + 10) { p->alive = 0; continue; }
		for (int b = 0; b < MAXBAL; b++)
			if (bal[b].alive && hypotf(p->x - bal[b].x, p->y - bal[b].y) < BALLOON_R + DART_R) {
				pop(&bal[b], p->dmg); spawn_pop(p->x, p->y); eng_sound_play(sfx_pop, 0.5f); p->alive = 0; break;
			}
	}

	if (lives <= 0) { lives = 0; st = S_DEAD; }
}

static void on_init(void)
{
	W = eng_width(); H = eng_height(); st = S_TITLE; sel_type = 0;
	cash = CASH0; lives = LIVES0; round_ = 1;         /* sane values so the title panel previews */
	img_balloon = eng_image_from_png("td/balloon.png");
	img_monkey  = eng_image_from_png("td/monkey.png");
	img_bg      = eng_image_from_png("td/bg.png");
	img_panel   = eng_image_from_png("td/panel.png");
	img_pop     = eng_image_from_png("td/pop.png");
	sfx_pop   = eng_sound_load("sfx/pop.wav");
	sfx_leak  = eng_sound_load("sfx/leak.wav");
	sfx_place = eng_sound_load("sfx/place.wav");
	sfx_ui    = eng_sound_load("sfx/ui.wav");
	sfx_start = eng_sound_load("sfx/start.wav");
	mus       = eng_sound_load("sfx/music.wav");
}

static void draw_thick(float ax, float ay, float bx, float by, int half, eng_color c)
{
	float dx = bx - ax, dy = by - ay, L = hypotf(dx, dy); if (L < 1) L = 1;
	float px = -dy / L, py = dx / L;
	for (int i = -half; i <= half; i++)
		eng_line((int)(ax + px * i), (int)(ay + py * i), (int)(bx + px * i), (int)(by + py * i), c);
}

static void draw_ring(float cx, float cy, float r, eng_color c)
{
	float prevx = cx + r, prevy = cy;
	for (int i = 1; i <= 28; i++) {
		float a = 2 * PI * i / 28, x = cx + cosf(a) * r, y = cy + sinf(a) * r;
		eng_line((int)prevx, (int)prevy, (int)x, (int)y, c);
		prevx = x; prevy = y;
	}
}

static void draw_pop(const Pop *p)                 /* play the "pow" burst sheet over its lifetime */
{
	int fr = (int)(p->t / POP_LIFE * POP_FRAMES);
	if (fr < 0) fr = 0; else if (fr >= POP_FRAMES) fr = POP_FRAMES - 1;
	eng_draw_image_cell(img_pop, (int)p->x, (int)p->y, POP_CELL, POP_CELL, fr, 0, ENG_WHITE);
}

static void draw_monkey(const Monkey *m)
{
	draw_thick(m->x, m->y, m->x + m->aimx * 24, m->y + m->aimy * 24, 2, C_DART);   /* blowgun toward aim */
	eng_draw_image(img_monkey, (int)m->x, (int)m->y, TYPES[m->type].tint);
}

static void draw_panel(void)
{
	char b[24];
	eng_text_aligned(PANEL_X + PANEL_W / 2, 8, 22, C_HUD, ENG_ALIGN_CENTER, "DEFENDERS");

	for (int i = 0; i < NTYPES; i++) {                 /* defender buttons */
		int top = type_btn_top(i), afford = cash >= TYPES[i].cost;
		eng_rect_fill(PBTN_X, top, PBTN_W, PBTN_H, i == sel_type ? ENG_RGB(74, 56, 38) : ENG_RGB(126, 96, 64));
		eng_rect(PBTN_X, top, PBTN_W, PBTN_H, i == sel_type ? C_OK : ENG_RGB(70, 50, 34));
		eng_draw_image(img_monkey, PBTN_X + 30, top + PBTN_H / 2, afford ? TYPES[i].tint : ENG_RGB(96, 96, 96));
		eng_text(PBTN_X + 56, top + 14, 22, afford ? C_HUD : C_MUTE, TYPES[i].name);
		snprintf(b, sizeof b, "$ %d", TYPES[i].cost);
		eng_text(PBTN_X + 56, top + 44, 20, afford ? C_WIN : C_NO, b);
	}

	snprintf(b, sizeof b, "$ %d", cash);                        eng_text(PBTN_X, 292, 26, C_WIN, b);
	snprintf(b, sizeof b, "LIVES %d", lives);                   eng_text(PBTN_X, 328, 22, C_HUD, b);
	snprintf(b, sizeof b, "ROUND %d/%d", round_, MAX_ROUND);    eng_text(PBTN_X, 360, 22, C_MUTE, b);

	int active = (st == S_READY);                      /* start-wave button */
	eng_rect_fill(START_X, START_Y, START_W, START_H, active ? ENG_RGB(40, 122, 62) : ENG_RGB(78, 78, 78));
	eng_rect(START_X, START_Y, START_W, START_H, active ? C_WIN : ENG_RGB(58, 58, 58));
	if (active) snprintf(b, sizeof b, "START WAVE");
	else        snprintf(b, sizeof b, "WAVE %d", round_);
	eng_text_aligned(START_X + START_W / 2, START_Y + 14, 22, C_HUD, ENG_ALIGN_CENTER, b);
}

static void on_draw_background(void)
{
	eng_draw_image(img_bg, PLAY_W / 2, H / 2, ENG_WHITE);                 /* baked grass + path */
	eng_draw_image(img_panel, PANEL_X + PANEL_W / 2, H / 2, ENG_WHITE);   /* wood shop panel */

	if (st == S_READY || st == S_PLAY || st == S_WIN || st == S_DEAD) {
		for (int i = 0; i < nmon; i++) draw_monkey(&mon[i]);
		for (int i = 0; i < MAXBAL; i++)
			if (bal[i].alive)
				eng_draw_image(img_balloon, (int)bal[i].x, (int)bal[i].y, BCOL[bal[i].layer]);
		for (int i = 0; i < MAXDART; i++)   /* a short streak trailing the dart's velocity */
			if (dart[i].alive)
				draw_thick(dart[i].x, dart[i].y,
				           dart[i].x - dart[i].vx * 0.03f, dart[i].y - dart[i].vy * 0.03f, 1, C_DART);
		for (int i = 0; i < MAXPOP; i++)
			if (pops[i].alive) draw_pop(&pops[i]);
	}

	if ((st == S_READY || st == S_PLAY) && sel_type >= 0) {  /* hover ghost + range ring (if a type is picked) */
		int px, py; eng_pointer(&px, &py);
		if (px >= 0 && px < PANEL_X) {
			int ok = can_build(px, py, sel_type);
			draw_ring(px, py, TYPES[sel_type].range, ok ? C_OK : C_NO);
			eng_draw_image(img_monkey, px, py, ok ? TYPES[sel_type].tint : C_NO);
		}
	}

	draw_panel();

	if (st == S_READY)
		eng_text_aligned(PLAY_W / 2, H - 30, 22, C_HUD, ENG_ALIGN_CENTER,
		                 "TAP THE GRASS TO PLACE, THEN START WAVE");
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(PLAY_W / 2, H / 2 - 84, 72, C_HUD, ENG_ALIGN_CENTER, "BALLOON TD");
		eng_text_aligned(PLAY_W / 2, H / 2 - 4, 24, C_MUTE, ENG_ALIGN_CENTER, "buy defenders, tap the grass to place them");
		eng_text_aligned(PLAY_W / 2, H / 2 + 30, 24, C_MUTE, ENG_ALIGN_CENTER, "pop balloons before they reach the end");
		eng_text_aligned(PLAY_W / 2, H / 2 + 74, 28, C_WIN, ENG_ALIGN_CENTER, "TAP / PRESS START");
	} else if (st == S_WIN) {
		eng_text_aligned(PLAY_W / 2, H / 2 - 40, 74, C_WIN, ENG_ALIGN_CENTER, "YOU WIN!");
		eng_text_aligned(PLAY_W / 2, H / 2 + 34, 26, C_MUTE, ENG_ALIGN_CENTER, "TAP TO CONTINUE");
	} else if (st == S_DEAD) {
		char b[32];
		eng_text_aligned(PLAY_W / 2, H / 2 - 40, 74, C_NO, ENG_ALIGN_CENTER, "OVERRUN!");
		snprintf(b, sizeof b, "REACHED ROUND %d", round_);
		eng_text_aligned(PLAY_W / 2, H / 2 + 30, 32, C_HUD, ENG_ALIGN_CENTER, b);
		eng_text_aligned(PLAY_W / 2, H / 2 + 74, 26, C_MUTE, ENG_ALIGN_CENTER, "TAP TO CONTINUE");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Balloon TD", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
