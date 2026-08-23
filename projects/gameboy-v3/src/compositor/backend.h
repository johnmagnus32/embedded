/* backend.h — the compositor's display + input backend seam.
 *
 * canvasd is backend-agnostic: it calls these five functions by name and never
 * learns which implementation it got. Exactly ONE backend_*.c is linked per build
 * (Makefile CANVAS_BACKEND):
 *   drm  -> backend_drm.c : the T113 — DRM/KMS dumb buffers + DE planes + evdev (real hw)
 *   web  -> backend_web.c : host dev — memfd buffers + software compose + a browser
 *                           over WebSocket (frames out, keys back). The simulator.
 *
 * Buffers: alloc returns an fd the client mmaps (dma-buf on drm, memfd on web); the
 * backend keeps its own mapping so present() can read the pixels. present(fg_fd)
 * shows the buffer identified by that fd (the foreground client's submitted one).
 */
#ifndef BACKEND_H
#define BACKEND_H
#include <stdint.h>

int  backend_init(uint32_t *w, uint32_t *h);            /* set up output; report geometry */
int  backend_alloc_buffer(uint32_t *stride, uint32_t *size);  /* -> fd for the client (-1 = none) */
void backend_present(int fg_fd);                        /* composite + show buffer fg_fd (-1 = nothing) */
int  backend_poll_input(uint32_t *button, int32_t *value);    /* 1 if an event was pending, else 0 */
void backend_fini(void);

#endif /* BACKEND_H */
