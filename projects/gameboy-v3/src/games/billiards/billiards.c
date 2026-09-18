/* games/billiards/billiards.c — Billiards: the game that completes the physics arc. Pinball
 * bounced a ball off STATIC things; here many EQUAL-MASS balls collide with each other and
 * exchange momentum (eng_resolve_circles — the two-movable-bodies impulse). Balls roll under
 * friction (no gravity, top-down), bounce off the rails (eng_circle_segment + eng_reflect), and
 * drop into pockets. All in fixed_update, substepped so fast balls don't tunnel or pass through
 * each other. Pure immediate-mode drawing (rects + discs + an aim line).
 *
 * Controls: LEFT/RIGHT aim, UP/DOWN power, A shoot the cue ball, START start/restart.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

#define PI 3.14159265f
enum { S_TITLE, S_AIM, S_ROLL, S_OVER };

#define R          11          /* ball radius */
#define MAXBALLS   8
#define LX  60
#define RX  740
#define TY  90
#define BY  440
#define POCKET_R   22
#define DRAG       0.992f      /* per-fixed-step rolling friction */
#define STOP       6.0f        /* speed below which a ball is parked */
#define MAXSHOT    950.0f
#define AIM_RATE   2.2f        /* rad/s */
#define PW_RATE    0.8f        /* power units/s */
#define RAIL_E     0.9f
#define BALL_E     0.96f

/* palette */
#define C_FELT   ENG_RGB(24, 110, 70)
#define C_RAIL   ENG_RGB(96, 60, 32)
#define C_POCKET ENG_RGB(8, 10, 14)
#define C_CUE    ENG_RGB(240, 244, 250)
#define C_LINE   ENG_RGB(230, 240, 255)
#define C_HUD    ENG_RGB(150, 220, 255)
#define C_MUTE   ENG_RGB(150, 170, 160)
#define C_WIN    ENG_RGB(120, 220, 140)

typedef struct { float x, y, vx, vy; int alive; eng_color col; } Ball;
typedef struct { float x0, y0, x1, y1; } Seg;

static int   W, H, st, shots;
static Ball  balls[MAXBALLS];
static int   nballs;
static float aim, power;

static const eng_color OBJCOL[] = {
	ENG_RGB(220, 200, 60), ENG_RGB(210, 70, 60), ENG_RGB(70, 120, 220),
	ENG_RGB(230, 140, 50), ENG_RGB(150, 90, 200), ENG_RGB(60, 180, 160),
};
#define NOBJ ((int)(sizeof(OBJCOL) / sizeof(OBJCOL[0])))

static const Seg rails[] = {
	{ LX, TY, RX, TY }, { LX, BY, RX, BY },   /* top, bottom */
	{ LX, TY, LX, BY }, { RX, TY, RX, BY },   /* left, right */
};
#define NRAIL ((int)(sizeof(rails) / sizeof(rails[0])))

static const float pockets[6][2] = {
	{ LX, TY }, { (LX + RX) / 2, TY }, { RX, TY },
	{ LX, BY }, { (LX + RX) / 2, BY }, { RX, BY },
};

static void place_cue(void) { balls[0].x = 240; balls[0].y = (TY + BY) / 2; balls[0].vx = balls[0].vy = 0; balls[0].alive = 1; balls[0].col = C_CUE; }

static void new_game(void)
{
	shots = 0;
	nballs = 1; place_cue();
	float s = 2 * R + 2, apex_x = 540, cy = (TY + BY) / 2;   /* rack: rows 1,2,3 apex toward the cue */
	for (int row = 0; row < 3; row++)
		for (int i = 0; i <= row; i++) {
			Ball *b = &balls[nballs];
			b->x = apex_x + row * s * 0.866f;
			b->y = cy + (i - row * 0.5f) * s;
			b->vx = b->vy = 0; b->alive = 1; b->col = OBJCOL[(nballs - 1) % NOBJ];
			nballs++;
		}
	aim = 0; power = 0.5f;
	st = S_AIM;
}

static void rail_collide(Ball *b)
{
	float nx, ny, pen;
	for (int i = 0; i < NRAIL; i++)
		if (eng_circle_segment(b->x, b->y, R, rails[i].x0, rails[i].y0, rails[i].x1, rails[i].y1,
		                       &nx, &ny, &pen)) {
			b->x += nx * pen; b->y += ny * pen;
			eng_reflect(&b->vx, &b->vy, nx, ny, RAIL_E);
		}
}

static void on_fixed(float fdt)
{
	if (st != S_ROLL) return;

	float smax = 0;
	for (int i = 0; i < nballs; i++)
		if (balls[i].alive) { float s = hypotf(balls[i].vx, balls[i].vy); if (s > smax) smax = s; }
	int steps = (int)(smax * fdt / 3.0f) + 1;   /* <= ~3px/micro-step: no tunneling through balls/rails */
	float h = fdt / steps;

	for (int s = 0; s < steps; s++) {
		for (int i = 0; i < nballs; i++)
			if (balls[i].alive) { balls[i].x += balls[i].vx * h; balls[i].y += balls[i].vy * h; rail_collide(&balls[i]); }
		for (int i = 0; i < nballs; i++)                     /* pairwise ball-ball momentum exchange */
			for (int j = i + 1; j < nballs; j++)
				if (balls[i].alive && balls[j].alive)
					eng_resolve_circles(&balls[i].x, &balls[i].y, &balls[i].vx, &balls[i].vy,
					                    &balls[j].x, &balls[j].y, &balls[j].vx, &balls[j].vy, R, BALL_E);
	}

	int moving = 0, scratch = 0;
	for (int i = 0; i < nballs; i++) {
		if (!balls[i].alive) continue;
		balls[i].vx *= DRAG; balls[i].vy *= DRAG;
		if (hypotf(balls[i].vx, balls[i].vy) < STOP) { balls[i].vx = balls[i].vy = 0; }
		for (int p = 0; p < 6; p++)                          /* pocketed? */
			if (hypotf(balls[i].x - pockets[p][0], balls[i].y - pockets[p][1]) < POCKET_R) {
				balls[i].alive = 0; if (i == 0) scratch = 1; break;
			}
		if (balls[i].alive && (balls[i].vx != 0 || balls[i].vy != 0)) moving = 1;
	}
	if (scratch) place_cue();                                /* scratch: cue ball comes back */

	if (!moving) {                                           /* everything settled */
		int obj = 0;
		for (int i = 1; i < nballs; i++) if (balls[i].alive) obj++;
		st = obj ? S_AIM : S_OVER;
	}
}

static void on_update(float dt)
{
	switch (st) {
	case S_TITLE: if (eng_just_pressed(ENG_START)) new_game(); break;
	case S_AIM:
		if (eng_pressed(ENG_LEFT))  aim -= AIM_RATE * dt;
		if (eng_pressed(ENG_RIGHT)) aim += AIM_RATE * dt;
		if (eng_pressed(ENG_UP))    power += PW_RATE * dt;
		if (eng_pressed(ENG_DOWN))  power -= PW_RATE * dt;
		if (power < 0.12f) power = 0.12f; else if (power > 1.0f) power = 1.0f;
		if (eng_just_pressed(ENG_A)) {                       /* strike the cue ball */
			balls[0].vx = cosf(aim) * power * MAXSHOT;
			balls[0].vy = sinf(aim) * power * MAXSHOT;
			shots++; st = S_ROLL;
		}
		break;
	case S_OVER: if (eng_just_pressed(ENG_START)) st = S_TITLE; break;
	default: break;
	}
}

static void on_init(void) { W = eng_width(); H = eng_height(); place_cue(); nballs = 1; st = S_TITLE; }

static void on_draw_background(void)
{
	char buf[24];
	eng_clear(ENG_RGB(18, 22, 30));
	eng_rect_fill(LX - 10, TY - 10, (RX - LX) + 20, (BY - TY) + 20, C_RAIL);   /* rail frame */
	eng_rect_fill(LX, TY, RX - LX, BY - TY, C_FELT);                           /* felt */
	for (int p = 0; p < 6; p++)
		eng_circle_fill((int)pockets[p][0], (int)pockets[p][1], POCKET_R, C_POCKET);
	for (int i = 0; i < nballs; i++)
		if (balls[i].alive) eng_circle_fill((int)balls[i].x, (int)balls[i].y, R, balls[i].col);

	if (st == S_AIM && balls[0].alive) {                                       /* aim line + power */
		float ex = balls[0].x + cosf(aim) * (40 + power * 150);
		float ey = balls[0].y + sinf(aim) * (40 + power * 150);
		eng_line((int)balls[0].x, (int)balls[0].y, (int)ex, (int)ey, C_LINE);
		eng_rect_fill(LX, BY + 16, (int)((RX - LX) * power), 8, C_HUD);
	}
	snprintf(buf, sizeof buf, "SHOTS %d", shots);
	eng_text(24, 24, 26, C_HUD, buf);
	int obj = 0; for (int i = 1; i < nballs; i++) if (balls[i].alive) obj++;
	snprintf(buf, sizeof buf, "BALLS %d", obj);
	eng_text_aligned(W - 24, 24, 26, C_HUD, ENG_ALIGN_RIGHT, buf);
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 70, 84, C_HUD, ENG_ALIGN_CENTER, "BILLIARDS");
		eng_text_aligned(W / 2, H / 2 + 20, 26, C_MUTE, ENG_ALIGN_CENTER, "LEFT/RIGHT AIM  UP/DOWN POWER  A SHOOT");
		eng_text_aligned(W / 2, H / 2 + 60, 30, C_CUE, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_OVER) {
		char buf[32];
		eng_text_aligned(W / 2, H / 2 - 50, 80, C_WIN, ENG_ALIGN_CENTER, "TABLE CLEARED!");
		snprintf(buf, sizeof buf, "IN %d SHOTS", shots);
		eng_text_aligned(W / 2, H / 2 + 20, 40, C_CUE, ENG_ALIGN_CENTER, buf);
		eng_text_aligned(W / 2, H / 2 + 70, 28, C_MUTE, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Billiards", .init = on_init, .update = on_update, .fixed_update = on_fixed,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
