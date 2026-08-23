/* libcanvas/canvas.c — client SDK implementation. The client side of canvas-proto.h:
 * connect, receive + mmap the compositor's buffers, track their free/in-flight
 * state, and drain input. Deliberately small; the compositor does the hard part.
 *
 * MOCK STATUS: the socket protocol + buffer bookkeeping are real. The buffers
 * arrive as dma-buf fds and are mmap'd directly (zero-copy — the compositor
 * scans out of this exact memory). Nothing here is stubbed.
 */
#include "canvas.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

enum buf_state { BUF_FREE, BUF_DRAWING, BUF_SUBMITTED };

struct canvas_ctx {
	int       sock;
	uint32_t  width, height, stride, nbufs;
	canvas_frame frames[CANVAS_MAX_BUFFERS];
	enum buf_state state[CANVAS_MAX_BUFFERS];
	int       focused;
};

canvas_ctx *canvas_connect(enum canvas_role role)
{
	canvas_ctx *c = calloc(1, sizeof(*c));
	if (!c) return NULL;

	c->sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (c->sock < 0) { free(c); return NULL; }

	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, canvas_sock_path(), sizeof(sa.sun_path) - 1);
	if (connect(c->sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) goto fail;

	struct canvas_msg m = { .op = CANVAS_HELLO, .u.hello.role = role };
	if (canvas_send(c->sock, &m, -1) < 0) goto fail;

	/* WELCOME, then nbufs x BUFFER (each carrying a dma-buf fd we mmap). */
	if (canvas_recv(c->sock, &m, NULL) < 0 || m.op != CANVAS_WELCOME) goto fail;
	c->width  = m.u.welcome.width;
	c->height = m.u.welcome.height;
	c->nbufs  = m.u.welcome.nbufs;
	if (c->nbufs > CANVAS_MAX_BUFFERS) goto fail;

	for (uint32_t i = 0; i < c->nbufs; i++) {
		int fd = -1;
		if (canvas_recv(c->sock, &m, &fd) < 0 || m.op != CANVAS_BUFFER || fd < 0) goto fail;
		uint32_t idx = m.u.buffer.index;
		void *p = mmap(NULL, m.u.buffer.size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, fd, 0);
		close(fd);
		if (p == MAP_FAILED) goto fail;
		c->stride = m.u.buffer.stride;
		c->frames[idx] = (canvas_frame){ .pixels = p, .width = c->width,
			.height = c->height, .stride = c->stride, .index = (int)idx };
		c->state[idx] = BUF_FREE;
	}
	return c;
fail:
	close(c->sock);
	free(c);
	return NULL;
}

void canvas_disconnect(canvas_ctx *c)
{
	if (!c) return;
	struct canvas_msg m = { .op = CANVAS_QUIT };
	canvas_send(c->sock, &m, -1);
	close(c->sock);
	free(c);
}

uint32_t canvas_width(const canvas_ctx *c)  { return c->width; }
uint32_t canvas_height(const canvas_ctx *c) { return c->height; }

canvas_frame *canvas_acquire(canvas_ctx *c)
{
	for (uint32_t i = 0; i < c->nbufs; i++)
		if (c->state[i] == BUF_FREE) {
			c->state[i] = BUF_DRAWING;
			return &c->frames[i];
		}
	return NULL;   /* all in flight — compositor owes us a RELEASE */
}

int canvas_submit(canvas_ctx *c, canvas_frame *f)
{
	c->state[f->index] = BUF_SUBMITTED;
	struct canvas_msg m = { .op = CANVAS_SUBMIT, .u.submit.index = (uint32_t)f->index };
	return canvas_send(c->sock, &m, -1);
}

int canvas_poll_input(canvas_ctx *c, canvas_input_event *ev)
{
	struct pollfd pfd = { .fd = c->sock, .events = POLLIN };
	while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
		struct canvas_msg m;
		if (canvas_recv(c->sock, &m, NULL) < 0) return 0;
		switch (m.op) {
		case CANVAS_RELEASE:
			c->state[m.u.release.index] = BUF_FREE;
			break;
		case CANVAS_FOCUS:
			c->focused = (int)m.u.focus.focused;
			break;
		case CANVAS_INPUT:
			if (ev) {
				ev->button = m.u.input.button;
				ev->value  = m.u.input.value;
				return 1;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}
