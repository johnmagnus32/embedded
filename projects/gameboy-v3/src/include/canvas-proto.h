/* canvas-proto.h — the gameboy-v3 compositor <-> client wire protocol (the spine).
 *
 * MODEL B (see pcb/ design notes): `canvasd` (the compositor) permanently owns
 * DRM master. It is the ONLY process that touches KMS. Every other process — the
 * launcher and native games — is a CLIENT that renders pixels into a
 * buffer the compositor allocated and hands it back to be shown. This header is
 * the contract between the two sides; both the compositor and libcanvas include it.
 *
 * Transport: one SEQPACKET unix socket per client (CANVAS_SOCK_PATH). Messages are
 * fixed-size `struct canvas_msg`. The one message that also carries a kernel object
 * — CANVAS_BUFFER, which ships a dma-buf fd — passes that fd out-of-band via
 * SCM_RIGHTS ancillary data (see canvas_send/canvas_recv below).
 *
 * Buffer lifecycle (the whole synchronization model, a tiny Wayland):
 *   1. client connects, sends CANVAS_HELLO{role}.
 *   2. compositor allocates N scanout buffers (DRM dumb buffers, panel-sized),
 *      replies CANVAS_WELCOME{w,h,format,nbufs}, then N x CANVAS_BUFFER{index,...}+fd.
 *   3. client mmaps each fd, draws into a free buffer, sends CANVAS_SUBMIT{index}.
 *   4. at the next vblank the compositor assigns each client's latest submitted
 *      buffer to a DE plane (game -> plane 0, overlay -> plane 1) and page-flips.
 *   5. when a buffer leaves scanout the compositor sends CANVAS_RELEASE{index};
 *      only then may the client reuse it (this is what stops cross-process
 *      tearing — the client can't overwrite a buffer still being scanned out).
 *
 * Input + focus: the compositor owns evdev (it owns the screen, so it owns focus,
 * exactly like a Wayland compositor). It filters system chords, then forwards
 * CANVAS_INPUT to the FOCUSED client. CANVAS_FOCUS tells a client it gained/lost focus.
 */
#ifndef CANVAS_PROTO_H
#define CANVAS_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define CANVAS_SOCK_PATH   "/run/canvas/session.sock"
#define CANVAS_MAX_BUFFERS 3          /* per client: draw one while another scans out */

/* Socket path with a CANVAS_SOCK env override — lets the host simulator use a
 * user-writable path (e.g. /tmp) instead of /run/canvas. Used by canvasd + libcanvas. */
static inline const char *canvas_sock_path(void)
{
	const char *p = getenv("CANVAS_SOCK");
	return (p && *p) ? p : CANVAS_SOCK_PATH;
}

/* Surface roles = which DE plane the compositor puts the client on. */
enum canvas_role {
	CANVAS_ROLE_GAME    = 0,      /* bottom plane: the launcher OR a running game  */
	CANVAS_ROLE_OVERLAY = 1,      /* top plane: the quick-menu, composited above   */
};

/* Pixel format of the scanout buffers. XRGB8888 is the DE's native UI-plane
 * format; keeping one format keeps clients and the compositor from negotiating. */
enum canvas_format {
	CANVAS_FMT_XRGB8888 = 0,
};

/* Canonical gamepad (the "RetroPad" abstraction). The compositor normalizes the
 * PCA9555 gpio-keys evdev stream into these codes so clients never see raw KEY_*
 * and per-model remaps live in one place. value = 1 press / 0 release. */
enum canvas_button {
	CANVAS_BTN_UP, CANVAS_BTN_DOWN, CANVAS_BTN_LEFT, CANVAS_BTN_RIGHT,
	CANVAS_BTN_A,  CANVAS_BTN_B,    CANVAS_BTN_X,    CANVAS_BTN_Y,
	CANVAS_BTN_L,  CANVAS_BTN_R,    CANVAS_BTN_START, CANVAS_BTN_SELECT,
	CANVAS_BTN__COUNT,
};

enum canvas_op {
	CANVAS_HELLO,     /* c->s: I am a client of role u.hello.role                  */
	CANVAS_WELCOME,   /* s->c: surface geometry + how many buffers follow          */
	CANVAS_BUFFER,    /* s->c: one buffer (index/stride/size) + a dma-buf fd       */
	CANVAS_SUBMIT,    /* c->s: buffer u.submit.index is drawn and ready to show    */
	CANVAS_RELEASE,   /* s->c: buffer u.release.index left scanout; reuse it       */
	CANVAS_INPUT,     /* s->c: a normalized gamepad event (u.input)                */
	CANVAS_FOCUS,     /* s->c: u.focus.focused = did I just gain (1) or lose (0)   */
	CANVAS_QUIT,      /* c->s: clean teardown (compositor also reaps on exit)      */
};

struct canvas_msg {
	uint32_t op;
	union {
		struct { uint32_t role; }                              hello;
		struct { uint32_t width, height, format, nbufs; }      welcome;
		struct { uint32_t index, stride, size; }               buffer;
		struct { uint32_t index; }                             submit;
		struct { uint32_t index; }                             release;
		struct { uint32_t button; int32_t value; }             input;
		struct { uint32_t focused; }                           focus;
	} u;
};

/* ---- wire helpers: fixed-size message + optional single fd via SCM_RIGHTS ----
 * static inline so both the compositor and libcanvas get them with zero link fuss.
 * passfd < 0 = no fd; on recv, *outfd is set to the received fd or -1. */

static inline int canvas_send(int sock, const struct canvas_msg *m, int passfd)
{
	struct iovec io = { (void *)m, sizeof(*m) };
	char cbuf[CMSG_SPACE(sizeof(int))];
	struct msghdr h = { 0 };
	h.msg_iov = &io; h.msg_iovlen = 1;
	if (passfd >= 0) {
		memset(cbuf, 0, sizeof(cbuf));
		h.msg_control = cbuf; h.msg_controllen = sizeof(cbuf);
		struct cmsghdr *c = CMSG_FIRSTHDR(&h);
		c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS;
		c->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(c), &passfd, sizeof(int));
	}
	return sendmsg(sock, &h, 0) == (ssize_t)sizeof(*m) ? 0 : -1;
}

static inline int canvas_recv(int sock, struct canvas_msg *m, int *outfd)
{
	struct iovec io = { m, sizeof(*m) };
	char cbuf[CMSG_SPACE(sizeof(int))];
	struct msghdr h = { 0 };
	h.msg_iov = &io; h.msg_iovlen = 1;
	h.msg_control = cbuf; h.msg_controllen = sizeof(cbuf);
	if (outfd) *outfd = -1;
	ssize_t n = recvmsg(sock, &h, 0);
	if (n <= 0) return -1;
	struct cmsghdr *c = CMSG_FIRSTHDR(&h);
	if (outfd && c && c->cmsg_type == SCM_RIGHTS)
		memcpy(outfd, CMSG_DATA(c), sizeof(int));
	return n == (ssize_t)sizeof(*m) ? 0 : -1;
}

#endif /* CANVAS_PROTO_H */
