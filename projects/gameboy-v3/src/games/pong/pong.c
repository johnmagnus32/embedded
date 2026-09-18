/* games/pong/pong.c — Pong: the game that proves the engine's new PHYSICS CORE (Step 1 toward
 * pinball). The ball is a CIRCLE with a velocity, integrated in fixed_update (the engine's
 * FIXED-timestep physics tick), and bounced off the walls and paddle faces with the new
 * eng_circle_segment + eng_reflect primitives — the exact circle/segment collision + reflection
 * pinball will be built on. A fast ball is SUBSTEPPED so it can't tunnel through a paddle.
 * Pure immediate-mode drawing (rects + eng_circle_fill); no sprites, no scene nodes.
 *
 * Left paddle = you (UP/DOWN). Right paddle = a simple tracking AI. A / START serves. First to 7.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

enum { S_TITLE, S_SERVE, S_PLAY, S_OVER };

#define BALL_R      8
#define PAD_W       14
#define PAD_H       92
#define PAD_MARGIN  32
#define PAD_SPD     460.0f
#define AI_SPD      360.0f     /* < player speed so the AI can be beaten */
#define BALL_SPD0   330.0f     /* serve speed */
#define BALL_MAX    720.0f     /* rally speed cap */
#define WIN         7

/* palette */
#define C_BG     ENG_RGB(10, 12, 20)      /* court background */
#define C_FG     ENG_RGB(235, 240, 255)   /* paddles, ball, scores */
#define C_MUTE   ENG_RGB(120, 130, 150)   /* net + hint text */
#define C_TITLE  ENG_RGB(150, 220, 255)   /* title / CPU-wins banner */
#define C_WIN    ENG_RGB(120, 220, 140)   /* you-win banner */

static int   W, H, st, pscore, ascore, serve_dir, serve_toggle;
static float player_y, ai_y;                            /* paddle top-edge y */
static float ball_x, ball_y, ball_vx, ball_vy, ball_speed;

static float player_face(void) { return PAD_MARGIN + PAD_W; }        /* player paddle's right face x */
static float ai_face(void)     { return W - PAD_MARGIN - PAD_W; }    /* AI paddle's left face x */
static void  center_ball(void) { ball_x = W / 2.0f; ball_y = H / 2.0f; ball_vx = ball_vy = 0; }

static void new_game(void)
{
	pscore = ascore = 0;
	player_y = ai_y = (H - PAD_H) / 2.0f;
	serve_dir = 1;
	center_ball();
	st = S_SERVE;
}

static void launch(void)
{
	ball_speed = BALL_SPD0;
	float a = serve_toggle ? 0.35f : -0.35f; serve_toggle = !serve_toggle;   /* alternate serve angle */
	ball_vx = serve_dir * ball_speed * cosf(a);
	ball_vy = ball_speed * sinf(a);
	st = S_PLAY;
}

static void score_point(int player_scored)
{
	if (player_scored) { pscore++; serve_dir =  1; }   /* next serve heads toward the loser's side */
	else               { ascore++; serve_dir = -1; }
	center_ball();
	st = (pscore >= WIN || ascore >= WIN) ? S_OVER : S_SERVE;
}

/* Recompute velocity leaving a paddle: bounce out along `dir`, steer by where it struck, speed up. */
static void off_paddle(float paddle_top, float dir)
{
	float rel = (ball_y - (paddle_top + PAD_H * 0.5f)) / (PAD_H * 0.5f);   /* -1 (top) .. 1 (bottom) */
	if (rel < -1.0f) rel = -1.0f; else if (rel > 1.0f) rel = 1.0f;
	ball_speed += 22.0f; if (ball_speed > BALL_MAX) ball_speed = BALL_MAX;  /* rallies get faster */
	float ang = rel * 0.9f;                                                 /* up to ~51 deg off horizontal */
	ball_vx = dir * ball_speed * cosf(ang);
	ball_vy = ball_speed * sinf(ang);
}

static void collide_ball(void)
{
	float nx, ny, pen;
	/* top + bottom walls (horizontal segments) — general primitive + perfect reflection */
	if (eng_circle_segment(ball_x, ball_y, BALL_R, 0, 0, (float)W, 0, &nx, &ny, &pen)) {
		ball_x += nx * pen; ball_y += ny * pen; eng_reflect(&ball_vx, &ball_vy, nx, ny, 1.0f);
	}
	if (eng_circle_segment(ball_x, ball_y, BALL_R, 0, (float)H, (float)W, (float)H, &nx, &ny, &pen)) {
		ball_x += nx * pen; ball_y += ny * pen; eng_reflect(&ball_vx, &ball_vy, nx, ny, 1.0f);
	}
	/* paddle front faces (vertical segments): detect with the primitive, custom paddle response */
	if (ball_vx < 0) {
		float fx = player_face();
		if (eng_circle_segment(ball_x, ball_y, BALL_R, fx, player_y, fx, player_y + PAD_H, &nx, &ny, &pen)) {
			ball_x += nx * pen; ball_y += ny * pen; off_paddle(player_y, +1.0f);
		}
	} else if (ball_vx > 0) {
		float fx = ai_face();
		if (eng_circle_segment(ball_x, ball_y, BALL_R, fx, ai_y, fx, ai_y + PAD_H, &nx, &ny, &pen)) {
			ball_x += nx * pen; ball_y += ny * pen; off_paddle(ai_y, -1.0f);
		}
	}
}

static void move_ball(float fdt)
{
	float speed = hypotf(ball_vx, ball_vy);
	int steps = (int)(speed * fdt / 4.0f) + 1;   /* <= ~4 px per micro-step -> can't tunnel a paddle */
	float h = fdt / steps;
	for (int i = 0; i < steps; i++) {
		ball_x += ball_vx * h;
		ball_y += ball_vy * h;
		collide_ball();
		if (ball_x < -BALL_R)    { score_point(0); return; }   /* past the player -> AI scores */
		if (ball_x > W + BALL_R) { score_point(1); return; }   /* past the AI -> player scores */
	}
}

static void on_fixed(float fdt)          /* FIXED-timestep physics (120 Hz), 0..N times per frame */
{
	if (st != S_PLAY && st != S_SERVE) return;

	if (eng_pressed(ENG_UP))   player_y -= PAD_SPD * fdt;
	if (eng_pressed(ENG_DOWN)) player_y += PAD_SPD * fdt;
	if (player_y < 0) player_y = 0; else if (player_y > H - PAD_H) player_y = H - PAD_H;

	float target = ball_y - PAD_H / 2.0f, step = AI_SPD * fdt;   /* AI chases the ball, capped */
	if      (ai_y < target - step) ai_y += step;
	else if (ai_y > target + step) ai_y -= step;
	else                           ai_y = target;
	if (ai_y < 0) ai_y = 0; else if (ai_y > H - PAD_H) ai_y = H - PAD_H;

	if (st == S_PLAY) move_ball(fdt);    /* the ball waits at center during SERVE */
}

static void on_update(float dt)
{
	(void)dt;
	switch (st) {
	case S_TITLE: if (eng_just_pressed(ENG_START)) new_game(); break;
	case S_SERVE: if (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_START)) launch(); break;
	case S_OVER:  if (eng_just_pressed(ENG_START)) st = S_TITLE; break;
	default: break;
	}
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	player_y = ai_y = (H - PAD_H) / 2.0f;
	center_ball();
	st = S_TITLE;
}

static void on_draw_background(void)
{
	char buf[16];
	eng_clear(C_BG);
	for (int y = 8; y < H; y += 28)                       /* dashed center net */
		eng_rect_fill(W / 2 - 2, y, 4, 16, C_MUTE);
	eng_rect_fill(PAD_MARGIN, (int)player_y, PAD_W, PAD_H, C_FG);
	eng_rect_fill(W - PAD_MARGIN - PAD_W, (int)ai_y, PAD_W, PAD_H, C_FG);
	if (st == S_PLAY || st == S_SERVE)
		eng_circle_fill((int)ball_x, (int)ball_y, BALL_R, C_FG);
	snprintf(buf, sizeof buf, "%d", pscore);
	eng_text_aligned(W / 2 - 70, 24, 56, C_FG, ENG_ALIGN_CENTER, buf);
	snprintf(buf, sizeof buf, "%d", ascore);
	eng_text_aligned(W / 2 + 70, 24, 56, C_FG, ENG_ALIGN_CENTER, buf);
}

static void on_draw_overlay(void)
{
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 60, 90, C_TITLE, ENG_ALIGN_CENTER, "PONG");
		eng_text_aligned(W / 2, H / 2 + 30, 28, C_MUTE, ENG_ALIGN_CENTER, "UP/DOWN MOVE    A SERVE");
		eng_text_aligned(W / 2, H / 2 + 70, 30, C_FG, ENG_ALIGN_CENTER, "PRESS START");
	} else if (st == S_SERVE) {
		eng_text_aligned(W / 2, H - 56, 26, C_MUTE, ENG_ALIGN_CENTER, "PRESS A TO SERVE");
	} else if (st == S_OVER) {
		int win = pscore >= WIN;
		eng_text_aligned(W / 2, H / 2 - 50, 80, win ? C_WIN : C_TITLE, ENG_ALIGN_CENTER,
		                 win ? "YOU WIN!" : "CPU WINS");
		eng_text_aligned(W / 2, H / 2 + 40, 30, C_FG, ENG_ALIGN_CENTER, "PRESS START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Pong", .init = on_init, .update = on_update, .fixed_update = on_fixed,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
