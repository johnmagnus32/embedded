/* backend_drm.c — the T113 display+input backend (DRM/KMS + evdev). The default
 * (CANVAS_BACKEND=drm); the flashable image builds this. STILL STUBBED — step D:
 *   init:  open("/dev/dri/card0"), drmSetMaster, find connector(panel)+mode+CRTC+2 planes
 *   alloc: DRM_IOCTL_MODE_CREATE_DUMB + ADDFB2 + PRIME_HANDLE_TO_FD -> the dma-buf fd
 *   present: drmModeSetPlane(game->0, overlay->1) + drmModePageFlip on vblank
 *   input: open the gpio-keys/power evdev nodes, map to canvas_button
 * Until then it reports the panel geometry and hands out no buffers (clients bail),
 * exactly the pre-existing behaviour — the web backend is where pixels happen today. */
#include "backend.h"

int backend_init(uint32_t *w, uint32_t *h) { *w = 800; *h = 480; return 0; }

int backend_alloc_buffer(uint32_t *stride, uint32_t *size)
{
	*stride = 800 * 4; *size = *stride * 480;
	return -1;   /* TODO: real dumb-buffer + dma-buf fd */
}

void backend_present(int fg_fd) { (void)fg_fd; /* TODO: setplane + pageflip */ }
int  backend_poll_input(uint32_t *b, int32_t *v) { (void)b; (void)v; return 0; /* TODO: evdev */ }
void backend_fini(void) { }
