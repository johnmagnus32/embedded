/* engine/platform/sdl/platform_sdl.c — a NATIVE desktop host platform (SDL2).
 *
 * Implements the engine platform seam (engine/runtime/engine_platform.h) with SDL2: it renders LOCALLY
 * into a window at the display's real (HiDPI) resolution — crisp and zero-latency. Cross-platform
 * (macOS / Linux / Windows). The engine core and editor/editor.c are untouched — the platform is a
 * link-time choice, so adding another host later never disturbs them. Self-registers via a constructor. */
#define _GNU_SOURCE
#include "engine.h"            /* ENG_* button codes, eng_quit */
#include "engine_platform.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EVQ 256

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t     *px;                 /* the engine's XRGB8888 draw buffer (acquire returns this) */
static int           pw, ph;             /* drawable size in PIXELS — what we render at (HiDPI-aware) */
static float         msx = 1, msy = 1;   /* window-point -> drawable-pixel scale (mouse coords) */
static eng_input_event evq[EVQ];
static int           eh, et;

static void evq_push(const eng_input_event *e) { int nt = (et + 1) % EVQ; if (nt == eh) return; evq[et] = *e; et = nt; }

/* map SDL keys to the engine's gamepad-style buttons */
static int map_key(SDL_Keycode k, uint32_t *b)
{
	switch (k) {
	case SDLK_LEFT:  *b = ENG_LEFT;  break;
	case SDLK_RIGHT: *b = ENG_RIGHT; break;
	case SDLK_UP:    *b = ENG_UP;    break;
	case SDLK_DOWN:  *b = ENG_DOWN;  break;
	case SDLK_z: case SDLK_SPACE:    *b = ENG_A; break;
	case SDLK_x:                     *b = ENG_B; break;
	case SDLK_a:                     *b = ENG_X; break;
	case SDLK_s:                     *b = ENG_Y; break;
	case SDLK_q:                     *b = ENG_L; break;
	case SDLK_w:                     *b = ENG_R; break;
	case SDLK_RETURN:                *b = ENG_START;  break;
	case SDLK_LSHIFT: case SDLK_RSHIFT: *b = ENG_SELECT; break;
	default: return 0;
	}
	return 1;
}

/* recompute drawable pixel size + the point->pixel mouse scale (call at open + on resize) */
static void refresh_size(void)
{
	int wpt, hpt;
	SDL_GetWindowSize(win, &wpt, &hpt);            /* logical points */
	SDL_GetRendererOutputSize(ren, &pw, &ph);      /* actual pixels (2x on Retina) */
	msx = wpt > 0 ? (float)pw / wpt : 1.0f;
	msy = hpt > 0 ? (float)ph / hpt : 1.0f;
}

/* window resized: re-fetch the pixel size, then reallocate the frame buffer + texture to match.
 * Runs during poll_input (before the frame's acquire/draw), so it never frees px mid-render. */
static void on_resize(void)
{
	int npw, nph, wpt, hpt;
	SDL_GetWindowSize(win, &wpt, &hpt);
	SDL_GetRendererOutputSize(ren, &npw, &nph);
	if (npw < 1 || nph < 1) return;
	uint32_t *np = realloc(px, (size_t)npw * nph * 4);
	if (!np) return;                       /* OOM on grow: leave px/pw/ph consistent at the old size */
	px = np; pw = npw; ph = nph;           /* commit new dims ONLY after the buffer actually grew */
	msx = wpt > 0 ? (float)pw / wpt : 1.0f;
	msy = hpt > 0 ? (float)ph / hpt : 1.0f;
	if (tex) SDL_DestroyTexture(tex);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, pw, ph);
}

static void pump_events(void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		eng_input_event ie; memset(&ie, 0, sizeof ie);
		switch (e.type) {
		case SDL_QUIT:
			eng_quit();
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) on_resize();
			break;
		case SDL_KEYDOWN:
			if (!e.key.repeat && map_key(e.key.keysym.sym, &ie.button)) { ie.type = ENG_EV_BUTTON; ie.value = 1; evq_push(&ie); }
			break;
		case SDL_KEYUP:
			if (map_key(e.key.keysym.sym, &ie.button)) { ie.type = ENG_EV_BUTTON; ie.value = 0; evq_push(&ie); }
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (e.button.button == SDL_BUTTON_LEFT) {
				ie.type = ENG_EV_POINTER;
				ie.value = (e.type == SDL_MOUSEBUTTONDOWN) ? 1 : 0;
				ie.x = (int)(e.button.x * msx); ie.y = (int)(e.button.y * msy);
				evq_push(&ie);
			}
			break;
		case SDL_MOUSEMOTION:
			ie.type = ENG_EV_POINTER; ie.value = 2;
			ie.x = (int)(e.motion.x * msx); ie.y = (int)(e.motion.y * msy);
			evq_push(&ie);
			break;
		case SDL_MOUSEWHEEL: {
			int wy = e.wheel.y;
			if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) wy = -wy;
			ie.type = ENG_EV_WHEEL; ie.value = -wy * 60;   /* scroll up = zoom in (matches web sign) */
			evq_push(&ie);
			break;
		}
		case SDL_MULTIGESTURE:                              /* trackpad pinch -> zoom */
			if (fabsf(e.mgesture.dDist) > 0.002f) {
				ie.type = ENG_EV_WHEEL; ie.value = (int)(-e.mgesture.dDist * 3000.0f);
				evq_push(&ie);
			}
			break;
		}
	}
}

/* ---- eng_platform hooks ---- */
static int s_open(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) { SDL_Log("platform_sdl: SDL_Init: %s", SDL_GetError()); return 1; }
	win = SDL_CreateWindow("engine editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                       1280, 800, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
	if (!win) { SDL_Log("platform_sdl: CreateWindow: %s", SDL_GetError()); return 1; }
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);   /* no vsync: eng_run already paces */
	if (!ren) { SDL_Log("platform_sdl: CreateRenderer: %s", SDL_GetError()); return 1; }
	refresh_size();
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, pw, ph);
	px = malloc((size_t)pw * ph * 4);
	if (!tex || !px) { SDL_Log("platform_sdl: alloc failed (%dx%d)", pw, ph); return 1; }
	memset(px, 0, (size_t)pw * ph * 4);
	SDL_Log("platform_sdl: %dx%d px window (scale %.2fx)", pw, ph, msx);
	return 0;
}
static int  s_width(void)  { return pw; }
static int  s_height(void) { return ph; }
static int  s_acquire(eng_surface *o) { o->pixels = px; o->width = pw; o->height = ph; o->stride = (uint32_t)pw * 4; return 1; }
static void s_present(const eng_surface *s)
{
	(void)s;
	if (!tex) return;
	SDL_UpdateTexture(tex, NULL, px, pw * 4);
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}
static int  s_poll_input(eng_input_event *e) { if (eh == et) pump_events(); if (eh == et) return 0; *e = evq[eh]; eh = (eh + 1) % EVQ; return 1; }
static void s_close(void)
{
	if (tex) SDL_DestroyTexture(tex);
	if (ren) SDL_DestroyRenderer(ren);
	if (win) SDL_DestroyWindow(win);
	free(px);
	SDL_Quit();
}

static const eng_platform SDL_PLATFORM = {
	s_open, s_width, s_height, s_acquire, s_present, s_poll_input, NULL, NULL, NULL, s_close,
};
__attribute__((constructor)) static void sdl_platform_register(void) { eng_set_platform(&SDL_PLATFORM); }
