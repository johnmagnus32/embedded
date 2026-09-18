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
#include "engine_internal.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

static const eng_platform *g_platform;      /* the registered platform (canvas compositor / host window) */
static eng_surface   g_surface;             /* the current frame's surface (filled by platform->acquire) */
static eng_surface  *g_frame;               /* -> g_surface while drawing, else NULL */
static bool          g_down[ENG_COUNT];      /* button held this frame */
static bool          g_edge[ENG_COUNT];      /* button went down THIS frame */
static bool          g_quit;
static int           g_ptr_x = -1, g_ptr_y = -1; /* last pointer/touch pos (-1 = none seen yet) */
static bool          g_ptr_down;             /* contact held this frame (persists across frames) */
static bool          g_ptr_edge_down;        /* contact BEGAN this frame (edge) */
static bool          g_ptr_edge_up;          /* contact ENDED this frame (edge) */
static float         g_wheel;                /* accumulated scroll/pinch delta, drained by eng_wheel() */

void eng_set_platform(const eng_platform *p) { g_platform = p; }   /* register before eng_run() */

int  eng_width(void)  { return g_platform ? g_platform->width()  : 0; }
int  eng_height(void) { return g_platform ? g_platform->height() : 0; }
void eng_quit(void)   { g_quit = true; }

eng_surface *eng__frame(void) { return g_frame; }   /* current draw target, for text.c */

bool eng_pressed(eng_button b)      { return b < ENG_COUNT && g_down[b]; }
bool eng_just_pressed(eng_button b) { return b < ENG_COUNT && g_edge[b]; }

bool eng_pointer_pressed(void)      { return g_ptr_down; }
bool eng_pointer_just_pressed(void) { return g_ptr_edge_down; }
bool eng_pointer_just_released(void){ return g_ptr_edge_up; }
void eng_pointer(int *x, int *y)    { if (x) *x = g_ptr_x; if (y) *y = g_ptr_y; }
float eng_wheel(void)               { float w = g_wheel; g_wheel = 0; return w; }

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

void eng_circle_fill(int cx, int cy, int r, eng_color c)   /* solid disc; scanline per row */
{
	if (!g_frame || r < 1) return;
	for (int dy = -r; dy <= r; dy++) {
		int y = cy + dy;
		if (y < 0 || y >= (int)g_frame->height) continue;
		int span = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f);   /* half chord width at this row */
		uint32_t *row = (uint32_t *)((char *)g_frame->pixels + (size_t)y * g_frame->stride);
		for (int x = cx - span; x <= cx + span; x++)
			if (x >= 0 && x < (int)g_frame->width) row[x] = c;
	}
}

void eng_line(int x0, int y0, int x1, int y1, eng_color c)   /* 1px Bresenham line */
{
	if (!g_frame) return;
	int dx =  (x1 > x0 ? x1 - x0 : x0 - x1), sx = x0 < x1 ? 1 : -1;
	int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		if (x0 >= 0 && x0 < (int)g_frame->width && y0 >= 0 && y0 < (int)g_frame->height)
			*((uint32_t *)((char *)g_frame->pixels + (size_t)y0 * g_frame->stride) + x0) = c;
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
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

void eng__blend(eng_surface *f, int x, int y, eng_color c, unsigned a)
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

/* Cheap post-process edge anti-aliasing (FXAA-style): softens jagged polygon silhouettes — road edge,
 * guardrail, kart outlines — in one pass over the finished frame, no supersampling. At each pixel it
 * measures the local luma range; only true edges (range >= threshold) get blended with the two pixels
 * ACROSS the edge, so flat interiors and textures stay crisp. Call after the 3D world, before the HUD
 * text (so glyphs stay sharp). Reads a snapshot so neighbour reads aren't already-blended. */
void eng_post_aa(void)
{
	eng_surface *f = g_frame; if (!f) return;
	int w = (int)f->width, h = (int)f->height; if (w < 3 || h < 3) return;
	static uint32_t *src; static int cap;
	if (cap < w*h) { void *p = realloc(src, (size_t)w*h*4); if (!p) return; src = p; cap = w*h; }
	for (int y = 0; y < h; y++)
		memcpy(src + (size_t)y*w, (char *)f->pixels + (size_t)y*f->stride, (size_t)w*4);
	#define CH(c,s) ((int)(((c)>>(s))&0xff))
	const int THRESH = 38;   /* per-CHANNEL range: catches equal-luma HUE edges (grey road vs green grass) a luma metric misses */
	for (int y = 1; y < h-1; y++) {
		uint32_t *out = (uint32_t *)((char *)f->pixels + (size_t)y*f->stride);
		uint32_t *r0 = src + (size_t)(y-1)*w, *r1 = src + (size_t)y*w, *r2 = src + (size_t)(y+1)*w;
		for (int x = 1; x < w-1; x++) {
			uint32_t M=r1[x], N=r0[x], S=r2[x], E=r1[x+1], Wp=r1[x-1];
			int range = 0;
			for (int sh = 0; sh < 24; sh += 8) {                 /* widest single-channel spread over the 5-tap cross */
				int mc=CH(M,sh), mx=mc, mn=mc;
				int a=CH(N,sh), b=CH(S,sh), c=CH(E,sh), d=CH(Wp,sh);
				mx = a>mx?a:mx; mx = b>mx?b:mx; mx = c>mx?c:mx; mx = d>mx?d:mx;
				mn = a<mn?a:mn; mn = b<mn?b:mn; mn = c<mn?c:mn; mn = d<mn?d:mn;
				if (mx-mn > range) range = mx-mn;
			}
			if (range < THRESH) continue;                        /* flat/interior/mild-texture — leave crisp */
			int gEW = abs(CH(E,16)-CH(Wp,16))+abs(CH(E,8)-CH(Wp,8))+abs(CH(E,0)-CH(Wp,0));
			int gNS = abs(CH(N,16)-CH(S,16))+abs(CH(N,8)-CH(S,8))+abs(CH(N,0)-CH(S,0));
			uint32_t t1,t2;
			if (gEW >= gNS) { t1=E; t2=Wp; }                     /* vertical-ish edge -> blend horizontally across it */
			else { t1=N; t2=S; }                                 /* horizontal-ish edge -> blend vertically */
			unsigned r=(CH(M,16)+CH(t1,16)+CH(t2,16))/3;
			unsigned g=(CH(M,8)+CH(t1,8)+CH(t2,8))/3;
			unsigned b=(CH(M,0)+CH(t1,0)+CH(t2,0))/3;
			out[x] = (r<<16)|(g<<8)|b;
		}
	}
	#undef CH
}

/* ---- the loop ---- */

/* Sample this frame's input: clear the edge bits, then drain the compositor's events
 * into the held/edge state. eng_button mirrors canvas_button 1:1, so the code casts. */
static void pump_input(void)
{
	memset(g_edge, 0, sizeof(g_edge));
	g_ptr_edge_down = g_ptr_edge_up = false;      /* edges reset each frame; held/pos persist */
	eng_input_event ev;
	while (g_platform->poll_input(&ev)) {
		if (ev.type == ENG_EV_POINTER) {          /* handle BEFORE the button-range guard below */
			g_ptr_x = ev.x; g_ptr_y = ev.y;   /* always track position (covers move/hover) */
			if (ev.value == 1) { if (!g_ptr_down) g_ptr_edge_down = true; g_ptr_down = true; }
			else if (ev.value == 0) { if (g_ptr_down) g_ptr_edge_up = true; g_ptr_down = false; }
			continue;
		}
		if (ev.type == ENG_EV_WHEEL) { g_wheel += (float)ev.value; continue; }
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
	if (!g_platform) return 1;                /* no platform registered (link a platform impl) */
	if (g_platform->open()) return 1;         /* open failed (e.g. no compositor reachable) */
	eng_font_init();                          /* load the vector font (text.c) */
	if (g_platform->audio_open) g_platform->audio_open();   /* connect audio sink (silent if unavailable) */
	if (g->init) g->init();

	struct timespec prev;
	clock_gettime(CLOCK_MONOTONIC, &prev);
	float acc = 0.0f;                         /* fixed-timestep accumulator (physics) */
	const float FDT = 1.0f / 120.0f;          /* physics tick: 120 Hz */

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
		if (g->fixed_update) {                /* run physics at a FIXED dt, 0..N times this frame */
			acc += dt;
			int guard = 0;                    /* cap steps so a slow frame can't spiral */
			while (acc >= FDT && guard++ < 8) { g->fixed_update(FDT); acc -= FDT; }
		}
		eng__scene_update(dt);                /* node _process callbacks, then reap freed */

		if (g_platform->acquire(&g_surface)) {
			g_frame = &g_surface;
			if (g->draw_background) g->draw_background();   /* immediate, behind the layers */
			eng__scene_render();                            /* BACKGROUND -> WORLD(camera) -> HUD */
			if (g->draw_overlay) g->draw_overlay();         /* immediate, on top of the layers */
			g_frame = NULL;
			g_platform->present(&g_surface);
		}
		if (g_platform->audio_feed) g_platform->audio_feed();   /* mix this frame's voices -> sink (every
		                                         iteration, even when no frame was acquired) */
		usleep(16000);                        /* ~60 Hz (TODO: pace to real vblank) */
	}

	eng_scene_clear();
	if (g_platform->audio_close) g_platform->audio_close();
	g_platform->close();
	return 0;
}
