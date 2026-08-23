/* games/breakout/breakout.c — Breakout: paddle, ball, a wall of bricks, a score/lives
 * HUD, and title/win/lose screens. The first canvas game with real game *states* and
 * on-screen *text* — it's what forced eng_text into the engine.
 *
 * Built entirely on the engine (eng_*): rectangles for the shapes, eng_text for the HUD.
 * No pixels, sockets, or loop here — that's the engine's job (game -> engine -> libcanvas
 * -> compositor). Physics is kept sqrt-free so the game needs no libm.
 *
 * Controls:  LEFT/RIGHT move the paddle,  A or START launch the ball,
 *            START (re)starts from the title / game-over / win screens.
 */
#include "engine.h"
#include <stdio.h>              /* snprintf for the HUD strings */

/* ---- layout (screen is 800x480 on the T113 panel) ---- */
#define COLS            10
#define ROWS            6
#define MARGIN_X        40      /* left/right gutter for the brick wall + HUD */
#define BRICK_TOP       74      /* first brick row Y (leaves room for the HUD) */
#define BRICK_H         24
#define GAP             6       /* gap between bricks */
#define PADDLE_W        112
#define PADDLE_H        16
#define PADDLE_MARGIN   40      /* paddle distance from the bottom edge */
#define BALL            12

enum state { S_TITLE, S_SERVE, S_PLAY, S_OVER, S_WIN };

static int   W, H;              /* screen size (from the engine at init) */
static int   brick_w;           /* computed to fill the wall width */
static enum  state st;

static float paddle_x;                          /* left edge */
static float ball_x, ball_y, ball_vx, ball_vy;  /* top-left + velocity (px/s) */
static bool  bricks[ROWS][COLS];
static int   bricks_left, score, lives;

/* one color per row, warm at the top -> cool at the bottom */
static const eng_color ROW_COLOR[ROWS] = {
	ENG_RGB(224, 64, 64),  ENG_RGB(232, 140, 44), ENG_RGB(230, 208, 52),
	ENG_RGB(72, 200, 88),  ENG_RGB(60, 176, 224), ENG_RGB(96, 112, 232),
};

static int paddle_top(void) { return H - PADDLE_MARGIN - PADDLE_H; }
static int brick_points(int row) { return (ROWS - row) * 10; }   /* top rows worth more */

static void rest_ball_on_paddle(void)
{
	ball_x  = paddle_x + PADDLE_W / 2.0f - BALL / 2.0f;
	ball_y  = (float)paddle_top() - BALL - 2.0f;
	ball_vx = ball_vy = 0.0f;
}

static void refill_bricks(void)
{
	bricks_left = 0;
	for (int r = 0; r < ROWS; r++)
		for (int c = 0; c < COLS; c++) { bricks[r][c] = true; bricks_left++; }
}

static void new_game(void)
{
	score = 0; lives = 3;
	paddle_x = (W - PADDLE_W) / 2.0f;
	refill_bricks();
	rest_ball_on_paddle();
	st = S_SERVE;
}

static void launch_ball(void)
{
	ball_vx = 150.0f;             /* mild initial angle; speed drifts a touch, Breakout-style */
	ball_vy = -320.0f;
	st = S_PLAY;
}

static void lose_life(void)
{
	if (--lives <= 0) st = S_OVER;
	else { rest_ball_on_paddle(); st = S_SERVE; }
}

/* Bounce the ball off any one brick it overlaps (one per frame keeps reflections sane). */
static void hit_bricks(void)
{
	float bl = ball_x, br = ball_x + BALL, bt = ball_y, bb = ball_y + BALL;
	for (int r = 0; r < ROWS; r++)
		for (int c = 0; c < COLS; c++) {
			if (!bricks[r][c]) continue;
			float x0 = (float)(MARGIN_X + c * (brick_w + GAP));
			float y0 = (float)(BRICK_TOP + r * (BRICK_H + GAP));
			if (br <= x0 || bl >= x0 + brick_w || bb <= y0 || bt >= y0 + BRICK_H) continue;

			/* reflect off the axis with the smaller penetration */
			float penX = (ball_vx > 0) ? (br - x0) : (x0 + brick_w - bl);
			float penY = (ball_vy > 0) ? (bb - y0) : (y0 + BRICK_H - bt);
			if (penX < penY) ball_vx = -ball_vx; else ball_vy = -ball_vy;

			bricks[r][c] = false;
			bricks_left--;
			score += brick_points(r);
			return;
		}
}

static void hit_paddle(void)
{
	int   py = paddle_top();
	float bl = ball_x, br = ball_x + BALL, bt = ball_y, bb = ball_y + BALL;
	if (ball_vy <= 0) return;                    /* only when descending */
	if (br <= paddle_x || bl >= paddle_x + PADDLE_W || bb <= py || bt >= py + PADDLE_H) return;

	ball_y  = (float)py - BALL;                  /* sit on top so it can't re-trigger */
	ball_vy = -(ball_vy < 0 ? -ball_vy : ball_vy);   /* send it upward */
	/* steer by where it struck: center = straight up, edges = steep sideways */
	float hit = (ball_x + BALL / 2.0f) - (paddle_x + PADDLE_W / 2.0f);
	ball_vx = hit / (PADDLE_W / 2.0f) * 300.0f;
	if (ball_vx >  360.0f) ball_vx =  360.0f;
	if (ball_vx < -360.0f) ball_vx = -360.0f;
}

/* ---- callbacks ---- */

static void on_init(void)
{
	W = eng_width();
	H = eng_height();
	brick_w = (W - 2 * MARGIN_X - (COLS - 1) * GAP) / COLS;
	paddle_x = (W - PADDLE_W) / 2.0f;
	score = 0; lives = 3;
	refill_bricks();                             /* a full wall as the title backdrop */
	rest_ball_on_paddle();
	st = S_TITLE;
}

static void on_update(float dt)
{
	if (st == S_SERVE || st == S_PLAY) {         /* paddle moves while in play */
		float sp = 620.0f;
		if (eng_pressed(ENG_LEFT))  paddle_x -= sp * dt;
		if (eng_pressed(ENG_RIGHT)) paddle_x += sp * dt;
		if (paddle_x < 0) paddle_x = 0;
		if (paddle_x > W - PADDLE_W) paddle_x = W - PADDLE_W;
	}

	switch (st) {
	case S_TITLE:
		if (eng_just_pressed(ENG_START)) new_game();
		break;
	case S_SERVE:
		rest_ball_on_paddle();                   /* ball rides the paddle until launch */
		if (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_START)) launch_ball();
		break;
	case S_PLAY:
		ball_x += ball_vx * dt;
		ball_y += ball_vy * dt;
		if (ball_x < 0)          { ball_x = 0;          ball_vx = -ball_vx; }
		if (ball_x > W - BALL)   { ball_x = W - BALL;   ball_vx = -ball_vx; }
		if (ball_y < 0)          { ball_y = 0;          ball_vy = -ball_vy; }
		hit_paddle();
		hit_bricks();
		if (bricks_left <= 0) st = S_WIN;
		else if (ball_y > H) lose_life();        /* fell past the bottom */
		break;
	case S_OVER:
	case S_WIN:
		if (eng_just_pressed(ENG_START)) st = S_TITLE;
		break;
	}
}

static void center_text(const char *s, int y, int px, eng_color c)
{
	eng_text((W - eng_text_width(px, s)) / 2, y, px, c, s);
}

static void on_draw_background(void)      /* breakout uses no scene nodes: it draws everything here */
{
	char buf[32];

	eng_clear(ENG_RGB(15, 15, 28));

	for (int r = 0; r < ROWS; r++)
		for (int c = 0; c < COLS; c++) {
			if (!bricks[r][c]) continue;
			int x0 = MARGIN_X + c * (brick_w + GAP);
			int y0 = BRICK_TOP + r * (BRICK_H + GAP);
			eng_rect_fill(x0, y0, brick_w, BRICK_H, ROW_COLOR[r]);
		}

	eng_rect_fill((int)paddle_x, paddle_top(), PADDLE_W, PADDLE_H, ENG_RGB(232, 232, 244));
	if (st == S_SERVE || st == S_PLAY)
		eng_rect_fill((int)ball_x, (int)ball_y, BALL, BALL, ENG_WHITE);

	snprintf(buf, sizeof buf, "SCORE %d", score);
	eng_text(MARGIN_X, 20, 30, ENG_WHITE, buf);
	snprintf(buf, sizeof buf, "LIVES %d", lives);
	eng_text(W - MARGIN_X - eng_text_width(30, buf), 20, 30, ENG_WHITE, buf);

	if (st == S_TITLE) {
		center_text("BREAKOUT", H / 2 - 90, 100, ENG_RGB(230, 208, 52));
		center_text("PRESS START", H / 2 + 40, 40, ENG_WHITE);
	} else if (st == S_OVER) {
		center_text("GAME OVER", H / 2 - 80, 84, ENG_RGB(224, 64, 64));
		snprintf(buf, sizeof buf, "SCORE %d", score);
		center_text(buf, H / 2 + 20, 44, ENG_WHITE);
		center_text("PRESS START", H / 2 + 84, 30, ENG_RGB(180, 180, 200));
	} else if (st == S_WIN) {
		center_text("YOU WIN!", H / 2 - 80, 84, ENG_RGB(72, 200, 88));
		snprintf(buf, sizeof buf, "SCORE %d", score);
		center_text(buf, H / 2 + 20, 44, ENG_WHITE);
		center_text("PRESS START", H / 2 + 84, 30, ENG_RGB(180, 180, 200));
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Breakout", .init = on_init, .update = on_update, .draw_background = on_draw_background,
	};
	return eng_run(&game);
}
