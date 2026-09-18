/* games/pinball/pinball.c — Pinball: STEP 2, the payoff of the physics core. A ball under gravity
 * bounces around a table of static segment walls and circle bumpers, and two FLIPPERS (rotating
 * segments) knock it back up — the flippers are MOVING colliders: on contact the ball is bounced in
 * the flipper's frame and the flipper's surface velocity is added back, so a swinging flipper kicks
 * the ball. Everything runs in fixed_update (120 Hz) and is SUBSTEPPED — the flippers and the ball
 * advance together in small steps so neither tunnels. Pure immediate-mode drawing (lines + discs).
 *
 * Controls: LEFT or L = left flipper, RIGHT or R = right flipper, A = launch, START = start/restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define PI 3.14159265f
enum { S_TITLE, S_READY, S_LIVE, S_OVER };

#define BALL_R      9
#define GRAV       1100.0f
#define BALL_MAX   1150.0f     /* speed cap (keeps substeps small, no tunneling) */
#define FLIP_SPEED   24.0f     /* flipper angular speed (rad/s) */
#define WALL_E       0.55f     /* wall restitution */
#define BUMP_E       0.92f     /* bumpers are bouncy */
#define BUMP_POP    130.0f     /* extra outward kick from a bumper */
#define BUMP_SCORE  100
#define FLIP_E       0.30f     /* flippers are not bouncy on their own — the kick comes from motion */
#define BALLS        3

/* palette */
#define C_BG     ENG_RGB(12, 14, 26)
#define C_WALL   ENG_RGB(90, 110, 170)
#define C_BUMP   ENG_RGB(220, 120, 60)
#define C_BUMP2  ENG_RGB(255, 200, 120)   /* bumper rim */
#define C_FLIP   ENG_RGB(210, 90, 120)
#define C_BALL   ENG_RGB(240, 244, 255)
#define C_HUD    ENG_RGB(150, 220, 255)
#define C_MUTE   ENG_RGB(120, 130, 150)
#define C_OVER   ENG_RGB(255, 120, 120)

typedef struct { float x0, y0, x1, y1; } Seg;
typedef struct { float x, y, r; } Circle;
typedef struct { float px, py, len, rest, active, a, omega; } Flipper;

static int W, H, st, score, balls;
static float bx, by, vx, vy;                 /* ball position + velocity */
static int   serve_toggle;

static const Seg walls[] = {
	{  16,  16, 784,  16 },   /* top */
	{  16,  16,  16, 464 },   /* left */
	{ 784,  16, 784, 464 },   /* right */
	{  16, 340, 295, 410 },   /* left funnel -> ONTO the left flipper pivot (no catch-pocket beside it) */
	{ 784, 340, 505, 410 },   /* right funnel -> ONTO the right flipper pivot */
};
#define NWALL ((int)(sizeof(walls) / sizeof(walls[0])))

static Circle bumpers[] = { { 250, 150, 26 }, { 550, 150, 26 }, { 400, 235, 28 } };
#define NBUMP ((int)(sizeof(bumpers) / sizeof(bumpers[0])))

/* rest angles point the tips down toward the center drain; active angles sweep them up */
static Flipper lflip = { 295, 410, 105,  0.436f, -0.349f,  0.436f, 0 };   /* +25deg -> -20deg */
static Flipper rflip = { 505, 410, 105,  2.705f,  3.491f,  2.705f, 0 };   /* 155deg -> 200deg */

static void flip_seg(const Flipper *f, float *ax, float *ay, float *bx2, float *by2)
{
	*ax = f->px; *ay = f->py;
	*bx2 = f->px + f->len * cosf(f->a);
	*by2 = f->py + f->len * sinf(f->a);
}

static void step_flipper(Flipper *f, int pressed, float h)
{
	float target = pressed ? f->active : f->rest, old = f->a, d = FLIP_SPEED * h;
	if      (f->a < target) { f->a += d; if (f->a > target) f->a = target; }
	else if (f->a > target) { f->a -= d; if (f->a < target) f->a = target; }
	f->omega = (f->a - old) / h;   /* 0 when at rest/target; nonzero while sweeping */
}

static void clamp_speed(void)
{
	float s2 = vx * vx + vy * vy;
	if (s2 > BALL_MAX * BALL_MAX) { float k = BALL_MAX / sqrtf(s2); vx *= k; vy *= k; }
}

static void hit_flipper(Flipper *f)   /* moving-collider bounce: reflect in the flipper frame + add its surface velocity */
{
	float ax, ay, bx2, by2, nx, ny, pen;
	flip_seg(f, &ax, &ay, &bx2, &by2);
	if (!eng_circle_segment(bx, by, BALL_R, ax, ay, bx2, by2, &nx, &ny, &pen)) return;
	bx += nx * pen; by += ny * pen;                      /* separate */
	float fdx = cosf(f->a), fdy = sinf(f->a);
	float rc = (bx - f->px) * fdx + (by - f->py) * fdy;  /* contact distance from the pivot */
	if (rc < 0) rc = 0; else if (rc > f->len) rc = f->len;
	float vsx = -fdy * f->omega * rc, vsy = fdx * f->omega * rc;   /* surface velocity at contact */
	float rvx = vx - vsx, rvy = vy - vsy;                /* ball velocity relative to the surface */
	eng_reflect(&rvx, &rvy, nx, ny, FLIP_E);
	vx = rvx + vsx; vy = rvy + vsy;                      /* back to world frame (the kick) */
	clamp_speed();
}

static void collide(void)
{
	float nx, ny, pen;
	for (int i = 0; i < NWALL; i++)
		if (eng_circle_segment(bx, by, BALL_R, walls[i].x0, walls[i].y0, walls[i].x1, walls[i].y1,
		                       &nx, &ny, &pen)) {
			bx += nx * pen; by += ny * pen; eng_reflect(&vx, &vy, nx, ny, WALL_E);
		}
	hit_flipper(&lflip);
	hit_flipper(&rflip);
	for (int i = 0; i < NBUMP; i++)
		if (eng_circle_circle(bx, by, BALL_R, bumpers[i].x, bumpers[i].y, bumpers[i].r,
		                      &nx, &ny, &pen)) {
			bx += nx * pen; by += ny * pen;
			eng_reflect(&vx, &vy, nx, ny, BUMP_E);
			vx += nx * BUMP_POP; vy += ny * BUMP_POP;    /* pop */
			score += BUMP_SCORE;
			clamp_speed();
		}
	clamp_speed();
}

static void spawn_ball(void) { bx = 400; by = 55; vx = vy = 0; }

static void drain(void)
{
	if (--balls <= 0) { st = S_OVER; return; }
	lflip.a = lflip.rest; rflip.a = rflip.rest;
	spawn_ball();
	st = S_READY;
}

static void on_fixed(float fdt)
{
	int lp = eng_pressed(ENG_LEFT)  || eng_pressed(ENG_L);
	int rp = eng_pressed(ENG_RIGHT) || eng_pressed(ENG_R);

	if (st == S_READY) {                 /* flippers respond, ball waits */
		step_flipper(&lflip, lp, fdt);
		step_flipper(&rflip, rp, fdt);
		return;
	}
	if (st != S_LIVE) return;

	/* substep flippers + ball together so a fast flipper/ball can't tunnel */
	float speed = hypotf(vx, vy);
	int by_ball  = (int)(speed * fdt / 3.0f) + 1;
	int by_flip  = (int)(fdt * lflip.len * FLIP_SPEED / 4.0f) + 1;
	int steps = by_ball > by_flip ? by_ball : by_flip;
	float h = fdt / steps;
	for (int i = 0; i < steps; i++) {
		step_flipper(&lflip, lp, h);
		step_flipper(&rflip, rp, h);
		vy += GRAV * h;
		bx += vx * h; by += vy * h;
		collide();
		if (by - BALL_R > (float)H) { drain(); return; }   /* fell out the bottom */
	}
}

static void new_game(void)
{
	score = 0; balls = BALLS;
	lflip.a = lflip.rest; rflip.a = rflip.rest;
	spawn_ball();
	st = S_READY;
}

static void launch(void)
{
	vy = 40.0f; vx = serve_toggle ? 70.0f : -70.0f; serve_toggle = !serve_toggle;
	st = S_LIVE;
}

static void on_update(float dt)
{
	(void)dt;
	switch (st) {
	case S_TITLE: if (eng_just_pressed(ENG_START)) new_game(); break;
	case S_READY: if (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_START)) launch(); break;
	case S_OVER:  if (eng_just_pressed(ENG_START)) st = S_TITLE; break;
	default: break;
	}
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	spawn_ball();
	st = S_TITLE;
}

static void draw_thick(float ax, float ay, float bx2, float by2, int half, eng_color c)
{
	float dx = bx2 - ax, dy = by2 - ay, L = hypotf(dx, dy); if (L < 1) L = 1;
	float px = -dy / L, py = dx / L;                 /* unit perpendicular */
	for (int i = -half; i <= half; i++)
		eng_line((int)(ax + px * i), (int)(ay + py * i), (int)(bx2 + px * i), (int)(by2 + py * i), c);
}

static void draw_flipper(const Flipper *f)
{
	float ax, ay, bx2, by2;
	flip_seg(f, &ax, &ay, &bx2, &by2);
	draw_thick(ax, ay, bx2, by2, 4, C_FLIP);
	eng_circle_fill((int)ax, (int)ay, 6, C_FLIP);
	eng_circle_fill((int)bx2, (int)by2, 5, C_FLIP);
}

static void on_draw_background(void)
{
	char buf[24];
	eng_clear(C_BG);
	for (int i = 0; i < NWALL; i++)
		draw_thick(walls[i].x0, walls[i].y0, walls[i].x1, walls[i].y1, 1, C_WALL);
	for (int i = 0; i < NBUMP; i++) {
		eng_circle_fill((int)bumpers[i].x, (int)bumpers[i].y, (int)bumpers[i].r, C_BUMP);
		eng_circle_fill((int)bumpers[i].x, (int)bumpers[i].y, (int)bumpers[i].r - 6, C_BUMP2);
	}
	draw_flipper(&lflip);
	draw_flipper(&rflip);
	if (st == S_READY || st == S_LIVE)
		eng_circle_fill((int)bx, (int)by, BALL_R, C_BALL);

	snprintf(buf, sizeof buf, "SCORE %d", score);
	eng_text(24, 22, 28, C_HUD, buf);
	snprintf(buf, sizeof buf, "BALLS %d", balls);
	eng_text_aligned(W - 24, 22, 28, C_HUD, ENG_ALIGN_RIGHT, buf);
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 70, 88, C_HUD, ENG_ALIGN_CENTER, "PINBALL");
		eng_text_aligned(W / 2, H / 2 + 24, 26, C_MUTE, ENG_ALIGN_CENTER, "LEFT/RIGHT FLIPPERS   A LAUNCH");
		eng_text_aligned(W / 2, H / 2 + 64, 30, C_BALL, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_READY) {
		eng_text_aligned(W / 2, H - 54, 26, C_MUTE, ENG_ALIGN_CENTER, "PRESS A TO LAUNCH");
	} else if (st == S_OVER) {
		char buf[32];
		eng_text_aligned(W / 2, H / 2 - 50, 80, C_OVER, ENG_ALIGN_CENTER, "GAME OVER");
		snprintf(buf, sizeof buf, "SCORE %d", score);
		eng_text_aligned(W / 2, H / 2 + 20, 40, C_BALL, ENG_ALIGN_CENTER, buf);
		eng_text_aligned(W / 2, H / 2 + 70, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Pinball", .init = on_init, .update = on_update, .fixed_update = on_fixed,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
