/* games/rhythm/rhythm.c — a 4-lane rhythm game. Notes fall to a hit line in time with the music;
 * tap the lane as a note lands for PERFECT/GOOD, miss and your combo resets. This is the first
 * game that needed a NEW engine primitive: eng_music_pos() (the audio playback clock), so notes
 * line up with what you HEAR. The visual clock is the frame timer gently RE-SYNCED to the audio
 * clock (minus a latency offset) — smooth frame-to-frame, drift-free over the song.
 *
 * Lanes L->R = LEFT / UP / DOWN / RIGHT (or tap the lane's column). TAP / A / START to begin.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

enum { S_TITLE, S_PLAY };

#define RLOOP    8.0f        /* rhythm.wav loop length (s); the chart repeats each loop */
#define OFFSET   0.09f       /* audio latency: heard = produced(eng_music_pos) - OFFSET (tunable) */
#define LEAD     1.5f        /* seconds a note is visible before its hit time (scroll time) */
#define PERFECT  0.055f
#define GOOD     0.110f
#define BEAT     0.5f        /* 120 BPM */

#define LANES    4
#define LANE_W   88
#define TOPY     40
#define MAXNOTE  64

static const unsigned char PAT[32] = {   /* per 1/8 slot (0.25s), bitmask of lanes (1=L 2=U 4=D 8=R) */
	1, 0, 2, 0, 4, 0, 8, 0,
	1, 0, 2, 4, 8, 0, 4, 2,
	1, 0, 2, 0, 4, 8, 0, 1,
	2, 0, 4, 0, 8, 1, 2, 4,
};
static const eng_color LANE_C[LANES] = {
	ENG_RGB(230, 70, 80), ENG_RGB(240, 200, 70), ENG_RGB(80, 210, 110), ENG_RGB(90, 150, 240),
};
static const eng_button LANE_K[LANES] = { ENG_LEFT, ENG_UP, ENG_DOWN, ENG_RIGHT };
static const char *LANE_L[LANES] = { "<", "^", "v", ">" };

typedef struct { float t; int lane, hit; } Note;    /* hit: 0 pending, 1 hit, 2 missed */
static int   W, H, st, hx0, hity, score, combo, bestcombo, nnote;
static float song_time, judg_t;
static const char *judg;
static eng_color judg_c;
static Note  notes[MAXNOTE];
static eng_sound *mus, *sfx_hit, *sfx_ui;

static float wrap(float d) { while (d < -RLOOP / 2) d += RLOOP; while (d > RLOOP / 2) d -= RLOOP; return d; }

static void build_chart(void)
{
	nnote = 0;
	for (int s = 0; s < 32 && nnote < MAXNOTE; s++)
		for (int l = 0; l < LANES; l++)
			if (PAT[s] & (1 << l)) notes[nnote++] = (Note){ s * 0.25f, l, 0 };
}
static void new_game(void)
{
	build_chart(); score = 0; combo = 0; song_time = 0; judg_t = 0; st = S_PLAY;
	eng_music_play(mus, 0.5f);
}
static void say(const char *t, eng_color c) { judg = t; judg_c = c; judg_t = 0.5f; }

static void judge(int lane)
{
	int best = -1; float bd = GOOD + 1;
	for (int i = 0; i < nnote; i++) {
		if (notes[i].hit || notes[i].lane != lane) continue;
		float d = fabsf(wrap(notes[i].t - song_time));
		if (d < bd) { bd = d; best = i; }
	}
	if (best < 0 || bd > GOOD) return;               /* nothing in the window — ignore */
	notes[best].hit = 1;
	if (bd <= PERFECT) { score += 100; say("PERFECT", ENG_RGB(255, 230, 90)); }
	else               { score += 50;  say("GOOD",    ENG_RGB(120, 220, 255)); }
	combo++; if (combo > bestcombo) bestcombo = combo;
	eng_sound_play(sfx_hit, 0.5f);
}

static void on_init(void)
{
	W = eng_width(); H = eng_height();
	hx0 = W / 2 - LANES * LANE_W / 2; hity = H - 96;
	mus     = eng_sound_load("sfx/rhythm.wav");
	sfx_hit = eng_sound_load("sfx/ui.wav");
	sfx_ui  = eng_sound_load("sfx/start.wav");
	st = S_TITLE;
}

static void on_update(float dt)
{
	if (judg_t > 0) judg_t -= dt;
	if (st == S_TITLE) {
		if (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_START) || eng_pointer_just_pressed()) {
			eng_sound_play(sfx_ui, 0.6f); new_game();
		}
		return;
	}
	/* advance the clock (frame timer, gently re-synced to the audio clock) */
	song_time += dt;
	double ap = eng_music_pos();
	if (ap > 0.0001) song_time += wrap((float)ap - OFFSET - song_time) * 0.08f;
	if (song_time >= RLOOP) { song_time -= RLOOP; for (int i = 0; i < nnote; i++) notes[i].hit = 0; }  /* loop */
	if (song_time < 0) song_time += RLOOP;

	/* input: a lane key OR a tap in that lane's column (edges -> read here, not fixed_update) */
	int tap = eng_pointer_just_pressed(); int px = -1, py;
	if (tap) eng_pointer(&px, &py);
	for (int l = 0; l < LANES; l++) {
		int hit = eng_just_pressed(LANE_K[l]);
		if (!hit && tap && px >= hx0 + l * LANE_W && px < hx0 + (l + 1) * LANE_W) hit = 1;
		if (hit) judge(l);
	}
	/* misses: a pending note that slipped past the window breaks the combo */
	for (int i = 0; i < nnote; i++)
		if (!notes[i].hit && wrap(notes[i].t - song_time) < -GOOD) {
			notes[i].hit = 2; combo = 0; say("MISS", ENG_RGB(230, 90, 90));
		}
}

static void on_draw_background(void)
{
	eng_clear(ENG_RGB(18, 20, 30));
	int hw = LANES * LANE_W;
	for (int l = 0; l < LANES; l++)                  /* lane columns */
		eng_rect_fill(hx0 + l * LANE_W, 0, LANE_W - 2, H, (l & 1) ? ENG_RGB(30, 33, 46) : ENG_RGB(26, 29, 40));

	int beatpulse = (fmodf(song_time, BEAT) < 0.08f && st == S_PLAY);   /* flash the bar on the beat */
	eng_rect_fill(hx0, hity - 4, hw, 8, beatpulse ? ENG_RGB(255, 255, 255) : ENG_RGB(150, 160, 190));
	for (int l = 0; l < LANES; l++) {                /* lane key hints at the hit line */
		int cx = hx0 + l * LANE_W + LANE_W / 2;
		eng_rect(cx - 26, hity + 8, 52, 40, LANE_C[l]);
		eng_text_aligned(cx, hity + 12, 30, LANE_C[l], ENG_ALIGN_CENTER, LANE_L[l]);
	}

	if (st == S_PLAY)                                /* falling notes */
		for (int i = 0; i < nnote; i++) {
			if (notes[i].hit == 1) continue;
			float d = wrap(notes[i].t - song_time);
			if (d < -GOOD || d > LEAD) continue;
			int cx = hx0 + notes[i].lane * LANE_W + LANE_W / 2;
			int y = (int)(hity - (d / LEAD) * (hity - TOPY));
			eng_color c = notes[i].hit == 2 ? ENG_RGB(90, 90, 100) : LANE_C[notes[i].lane];
			eng_rect_fill(cx - (LANE_W / 2 - 10), y - 11, LANE_W - 20, 22, c);
			eng_rect(cx - (LANE_W / 2 - 10), y - 11, LANE_W - 20, 22, ENG_RGB(255, 255, 255));
		}
}

static void on_draw_overlay(void)
{
	char buf[32];
	snprintf(buf, sizeof buf, "SCORE %d", score);   eng_text(16, 12, 28, ENG_RGB(255, 255, 255), buf);
	snprintf(buf, sizeof buf, "COMBO %d", combo);    eng_text_aligned(W - 16, 12, 28, ENG_RGB(255, 230, 120), ENG_ALIGN_RIGHT, buf);
	if (judg_t > 0)
		eng_text_aligned(W / 2, H / 2 - 40, 46, judg_c, ENG_ALIGN_CENTER, judg);
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 80, 72, ENG_RGB(255, 255, 255), ENG_ALIGN_CENTER, "RHYTHM");
		eng_text_aligned(W / 2, H / 2 + 2, 24, ENG_RGB(200, 210, 230), ENG_ALIGN_CENTER, "tap the lane as the note hits the line");
		eng_text_aligned(W / 2, H / 2 + 38, 24, ENG_RGB(200, 210, 230), ENG_ALIGN_CENTER, "keys: LEFT UP DOWN RIGHT  (or tap a lane)");
		eng_text_aligned(W / 2, H / 2 + 82, 28, ENG_RGB(120, 220, 150), ENG_ALIGN_CENTER, "TAP / A TO START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Rhythm", .init = on_init, .update = on_update,
		.draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
