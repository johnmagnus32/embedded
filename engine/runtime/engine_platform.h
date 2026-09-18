/* engine/runtime/engine_platform.h — the PLATFORM SEAM.
 *
 * The engine core (runtime/) touches the outside world — framebuffer, input, audio — ONLY through
 * this interface. A front-end fills an eng_platform and registers it (usually via a constructor)
 * BEFORE eng_run(): the canvas compositor client on the device/sim, or a host window for the editor.
 * This is what makes the runtime standalone — it no longer #includes canvas.h / sound.h. */
#ifndef ENGINE_PLATFORM_H
#define ENGINE_PLATFORM_H
#include <stdint.h>

/* A drawable surface the rasterizers write into: XRGB8888, row-major, `stride` bytes/row.
 * (Fields mirror the compositor's frame so a canvas platform maps it with zero copy.) */
typedef struct { uint32_t *pixels; uint32_t width, height, stride; } eng_surface;

/* One input event, engine-owned (decoupled from the compositor). */
enum { ENG_EV_BUTTON, ENG_EV_POINTER, ENG_EV_WHEEL };   /* type */
typedef struct {
	uint32_t type;      /* ENG_EV_BUTTON | ENG_EV_POINTER | ENG_EV_WHEEL */
	uint32_t button;    /* eng_button code (when ENG_EV_BUTTON) */
	int32_t  value;     /* button: 1/0; pointer: phase (1 down / 0 up / 2 move); wheel: signed delta */
	int32_t  x, y;      /* pointer surface coords */
} eng_input_event;

/* Software mixer output (engine/audio.c). A platform's audio transport pulls `frames` interleaved
 * stereo int16 frames on demand and writes them to its sink. Declared here so platform impls can
 * pull it without reaching into engine internals. */
#define ENG_MIX_RATE 44100
#define ENG_MIX_CH   2
void eng__mix(int16_t *out, int frames);

/* The platform the engine runs on. audio_* may be NULL (runs silent). */
typedef struct {
	int  (*open)(void);                    /* connect/open; 0 ok, nonzero = fail (eng_run returns 1) */
	int  (*width)(void);
	int  (*height)(void);
	int  (*acquire)(eng_surface *out);     /* fill *out with the next frame; 1 = got one, 0 = none this tick */
	void (*present)(const eng_surface *);  /* submit the drawn frame */
	int  (*poll_input)(eng_input_event *); /* drain one event; 1 filled, 0 empty */
	void (*audio_open)(void);
	void (*audio_feed)(void);              /* pull eng__mix -> sink for this frame */
	void (*audio_close)(void);
	void (*close)(void);
} eng_platform;

void eng_set_platform(const eng_platform *p);   /* register before eng_run() */

#endif /* ENGINE_PLATFORM_H */
