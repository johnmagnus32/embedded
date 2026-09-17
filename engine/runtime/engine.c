/* engine/engine.c — the 2D game engine implementation (first slice).
 *
 * Owns the plumbing a game used to write by hand: connect to the compositor, run the
 * ~60 Hz loop (sample input -> update(dt) -> draw into a frame -> submit), and pace.
 * The drawing calls (eng_clear/eng_rect_fill/eng_rect) are REAL CPU pixel writes into
 * the acquired frame — the same work a game used to do inline, now behind an API.
 * Built on the canvas SDK (libcanvas); game code never sees canvas_* directly.
 * (Text lives in text.c; it draws into the same frame via eng__frame().)
 *
 * STATUS: fully real. Runs end-to-end whenever `canvasd` hands out buffers — the web sim
 * backend does today; on the T113, once backend_drm's KMS is real. eng_run() returns 1
 * only when it can't reach a compositor at all.
 */
#include "engine.h"
#include "canvas.h"
#include "engine_internal.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

static canvas_ctx   *g_ctx;                 /* compositor connection */
static canvas_frame *g_frame;               /* the frame being drawn — valid only during draw() */
static bool          g_down[ENG_COUNT];      /* button held this frame */
static bool          g_edge[ENG_COUNT];      /* button went down THIS frame */
static bool          g_quit;

int  eng_width(void)  { return g_ctx ? (int)canvas_width(g_ctx)  : 0; }
int  eng_height(void) { return g_ctx ? (int)canvas_height(g_ctx) : 0; }
void eng_quit(void)   { g_quit = true; }

canvas_frame *eng__frame(void) { return g_frame; }   /* current draw target, for text.c */

bool eng_pressed(eng_button b)      { return b < ENG_COUNT && g_down[b]; }
bool eng_just_pressed(eng_button b) { return b < ENG_COUNT && g_edge[b]; }

/* ---- drawing: real pixel writes into the current frame (XRGB8888, row-major) ---- */

void eng_clear(eng_color c)
{
	if (!g_frame) return;
	for (uint32_t y = 0; y < g_frame->height; y++) {
		uint32_t *row = (uint32_t *)((char *)g_frame->pixels + y * g_frame->stride);
		for (uint32_t x = 0; x < g_frame->width; x++) row[x] = c;
	}
}

void eng_rect_fill(int x, int y, int w, int h, eng_color c)
{
	if (!g_frame) return;
	for (int j = y; j < y + h; j++) {
		if (j < 0 || j >= (int)g_frame->height) continue;
		uint32_t *row = (uint32_t *)((char *)g_frame->pixels + j * g_frame->stride);
		for (int i = x; i < x + w; i++)
			if (i >= 0 && i < (int)g_frame->width) row[i] = c;
	}
}

void eng_rect(int x, int y, int w, int h, eng_color c)   /* 1px outline = four thin fills */
{
	eng_rect_fill(x,         y,         w, 1, c);   /* top    */
	eng_rect_fill(x,         y + h - 1, w, 1, c);   /* bottom */
	eng_rect_fill(x,         y,         1, h, c);   /* left   */
	eng_rect_fill(x + w - 1, y,         1, h, c);   /* right  */
}

/* ---- gamma-correct alpha compositing (shared by text.c + scene.c) -------------------
 * Frame values are sRGB (non-linear). Blending coverage directly on those bytes mixes in
 * the wrong space and looks muddy, so convert src+dst to LINEAR light, blend there, and
 * convert back. Lookup tables (built once) keep it to table reads — no per-pixel pow(). */
#define LIN2S 4096
static float   s_srgb2lin[256];       /* sRGB byte -> linear [0,1] */
static uint8_t s_lin2srgb[LIN2S];     /* linear [0,1] (quantized) -> sRGB byte */
static int     s_gamma_ready;

static void gamma_init(void)
{
	for (int i = 0; i < 256; i++) {
		float v = i / 255.0f;
		s_srgb2lin[i] = (v <= 0.04045f) ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
	}
	for (int i = 0; i < LIN2S; i++) {
		float l = i / (float)(LIN2S - 1);
		float v = (l <= 0.0031308f) ? l * 12.92f : 1.055f * powf(l, 1.0f / 2.4f) - 0.055f;
		int q = (int)(v * 255.0f + 0.5f);
		s_lin2srgb[i] = (uint8_t)(q < 0 ? 0 : q > 255 ? 255 : q);
	}
	s_gamma_ready = 1;
}

static inline uint8_t lin2srgb(float l)
{
	if (l < 0.0f) l = 0.0f;
	if (l > 1.0f) l = 1.0f;
	return s_lin2srgb[(int)(l * (LIN2S - 1) + 0.5f)];
}

void eng__blend(canvas_frame *f, int x, int y, eng_color c, unsigned a)
{
	if (a == 0 || x < 0 || y < 0 || x >= (int)f->width || y >= (int)f->height) return;
	if (!s_gamma_ready) gamma_init();
	uint32_t *p = (uint32_t *)((char *)f->pixels + (size_t)y * f->stride) + x;
	uint32_t d = *p;
	float af = a / 255.0f, na = 1.0f - af;
	float r = s_srgb2lin[(c >> 16) & 0xff] * af + s_srgb2lin[(d >> 16) & 0xff] * na;
	float g = s_srgb2lin[(c >>  8) & 0xff] * af + s_srgb2lin[(d >>  8) & 0xff] * na;
	float b = s_srgb2lin[ c        & 0xff] * af + s_srgb2lin[ d        & 0xff] * na;
	*p = ((uint32_t)lin2srgb(r) << 16) | ((uint32_t)lin2srgb(g) << 8) | lin2srgb(b);
}

/* ---- the loop ---- */

/* Sample this frame's input: clear the edge bits, then drain the compositor's events
 * into the held/edge state. eng_button mirrors canvas_button 1:1, so the code casts. */
static void pump_input(void)
{
	memset(g_edge, 0, sizeof(g_edge));
	canvas_input_event ev;
	while (canvas_poll_input(g_ctx, &ev)) {
		if (ev.button >= ENG_COUNT) continue;
		if (ev.value) {                      /* press */
			if (!g_down[ev.button]) g_edge[ev.button] = true;
			g_down[ev.button] = true;
		} else {                             /* release */
			g_down[ev.button] = false;
		}
	}
}

int eng_run(const eng_game *g)
{
	g_ctx = canvas_connect(CANVAS_ROLE_GAME);
	if (!g_ctx) return 1;                     /* no compositor reachable */
	eng_font_init();                          /* load the vector font (text.c) */
	if (g->init) g->init();

	struct timespec prev;
	clock_gettime(CLOCK_MONOTONIC, &prev);

	while (!g_quit) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		float dt = (float)(now.tv_sec - prev.tv_sec) + (float)(now.tv_nsec - prev.tv_nsec) / 1e9f;
		prev = now;
		if (dt > 0.05f) dt = 0.05f;   /* max-timestep guard: one clamp for BOTH update(dt) and the
		                                 scene node updates below, so a post-suspend/stall spike can't
		                                 tunnel movement through walls or fast-forward timers. */

		pump_input();
		if (g->update) g->update(dt);
		eng__scene_update(dt);                /* node _process callbacks, then reap freed */

		canvas_frame *f = canvas_acquire(g_ctx);
		if (f) {
			g_frame = f;
			if (g->draw_background) g->draw_background();   /* immediate, behind the layers */
			eng__scene_render();                            /* BACKGROUND -> WORLD(camera) -> HUD */
			if (g->draw_overlay) g->draw_overlay();         /* immediate, on top of the layers */
			g_frame = NULL;
			canvas_submit(g_ctx, f);
		}
		usleep(16000);                        /* ~60 Hz (TODO: pace to real vblank) */
	}

	eng_scene_clear();
	canvas_disconnect(g_ctx);
	g_ctx = NULL;
	return 0;
}
