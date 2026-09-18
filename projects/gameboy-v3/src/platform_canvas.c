/* projects/gameboy-v3/src/platform_canvas.c — the CANVAS platform for the engine.
 *
 * Implements the engine's platform seam (engine/runtime/engine_platform.h) on top of libcanvas (the
 * compositor client) + libsound (the soundd audio transport). It SELF-REGISTERS via a constructor, so
 * games just link this file and call eng_run() as before — the engine core carries no canvas/sound
 * dependency. This is the gameboy-v3-specific glue; a host editor would provide its own platform. */
#include "engine_platform.h"
#include "canvas.h"
#include "sound.h"
#include <stdint.h>

static canvas_ctx   *g_ctx;
static canvas_frame *g_cur;      /* the in-flight frame between acquire() and present() */
static sound_ctx    *g_snd;

static int  cp_open(void)   { g_ctx = canvas_connect(CANVAS_ROLE_GAME); return g_ctx ? 0 : 1; }
static int  cp_width(void)  { return g_ctx ? (int)canvas_width(g_ctx)  : 0; }
static int  cp_height(void) { return g_ctx ? (int)canvas_height(g_ctx) : 0; }

static int cp_acquire(eng_surface *out)
{
	g_cur = canvas_acquire(g_ctx);
	if (!g_cur) return 0;
	out->pixels = g_cur->pixels; out->width = g_cur->width; out->height = g_cur->height; out->stride = g_cur->stride;
	return 1;
}
static void cp_present(const eng_surface *s) { (void)s; if (g_cur) canvas_submit(g_ctx, g_cur); g_cur = NULL; }

static int cp_poll(eng_input_event *e)
{
	canvas_input_event ce;
	if (!canvas_poll_input(g_ctx, &ce)) return 0;
	e->type   = (ce.type == CANVAS_POINTER) ? ENG_EV_POINTER : ENG_EV_BUTTON;
	e->button = ce.button; e->value = ce.value; e->x = ce.x; e->y = ce.y;
	return 1;
}

/* Audio transport: feed the engine's mixed stream into soundd's ring, headroom-driven. */
#define MIX_MAX 4096
static void cp_audio_open(void)  { g_snd = sound_connect(SOUND_ROLE_GAME); }   /* NULL -> stays silent */
static void cp_audio_feed(void)
{
	if (!g_snd) return;
	sound_poll(g_snd);                        /* learn how far soundd has drained */
	int n = sound_avail(g_snd);
	if (n <= 0) return;
	if (n > MIX_MAX) n = MIX_MAX;
	int16_t buf[MIX_MAX * ENG_MIX_CH];
	eng__mix(buf, n);                         /* pull the engine's per-game mix */
	sound_write(g_snd, buf, n);
}
static void cp_audio_close(void) { if (g_snd) { sound_disconnect(g_snd); g_snd = NULL; } }
static void cp_close(void)       { if (g_ctx) { canvas_disconnect(g_ctx); g_ctx = NULL; } }

static const eng_platform CANVAS_PLATFORM = {
	cp_open, cp_width, cp_height, cp_acquire, cp_present, cp_poll,
	cp_audio_open, cp_audio_feed, cp_audio_close, cp_close,
};

__attribute__((constructor)) static void canvas_platform_register(void) { eng_set_platform(&CANVAS_PLATFORM); }
