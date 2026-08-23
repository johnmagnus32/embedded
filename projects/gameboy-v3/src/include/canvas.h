/* canvas.h — the gameboy-v3 client SDK (libcanvas) public API.
 *
 * Everything a client needs to be a compositor client: connect, get a buffer to
 * draw into, submit it, and read input. A native game, the launcher, and
 * canvas-retro all link this and use nothing else to reach the screen. The SDK hides
 * the canvas-proto.h wire protocol and the dma-buf mmap bookkeeping.
 *
 * The draw model is deliberately dumb — the T113 has no GPU, so a client fills a
 * CPU-mapped XRGB8888 buffer directly. That's it; the DE + compositor do the rest.
 */
#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "canvas-proto.h"

typedef struct canvas_ctx canvas_ctx;   /* opaque connection handle */

/* A buffer to draw into. `pixels` is the mmap'd XRGB8888 surface; write
 * width*height pixels at `stride` bytes/row, then canvas_submit() it. */
typedef struct {
	uint32_t *pixels;
	uint32_t  width, height;   /* pixels */
	uint32_t  stride;          /* bytes per row (>= width*4) */
	int       index;           /* buffer id, SDK-internal */
} canvas_frame;

/* An input event as delivered to a client (a subset of canvas_msg.u.input). */
typedef struct {
	uint32_t button;   /* enum canvas_button */
	int32_t  value;    /* 1 = pressed, 0 = released */
} canvas_input_event;

/* Connect to the compositor as `role` (CANVAS_ROLE_GAME for the launcher/games,
 * CANVAS_ROLE_OVERLAY for the quick-menu). Blocks until WELCOME + buffers arrive.
 * Returns NULL on failure. */
canvas_ctx *canvas_connect(enum canvas_role role);
void     canvas_disconnect(canvas_ctx *c);

uint32_t canvas_width(const canvas_ctx *c);
uint32_t canvas_height(const canvas_ctx *c);

/* Acquire a free buffer to draw into. Returns NULL if all buffers are still
 * in flight (compositor hasn't RELEASE'd one yet) — try again next tick. */
canvas_frame *canvas_acquire(canvas_ctx *c);

/* Present a drawn frame. After this the buffer is owned by the compositor until
 * it sends a RELEASE (surfaced back through canvas_acquire). */
int canvas_submit(canvas_ctx *c, canvas_frame *f);

/* Non-blocking input drain: returns 1 and fills *ev if an event was pending,
 * 0 if none. Also services RELEASE/FOCUS bookkeeping. Call every frame. */
int canvas_poll_input(canvas_ctx *c, canvas_input_event *ev);

/* (Launching a game is NOT a compositor op — it's a process-lifecycle action owned
 * by appletd; see appletd-proto.h. The launcher talks to appletd directly.) */

#endif /* CANVAS_H */
