/* backend_web.c — the HOST simulator backend (CANVAS_BACKEND=web). Instead of the
 * T113 panel, canvasd renders into a browser tab:
 *   - alloc  : client buffers are memfd (mmap'd; same SCM_RIGHTS fd-passing as dma-buf,
 *              so libcanvas + every client are byte-for-byte unchanged),
 *   - present: software-composite the foreground buffer into an RGBA framebuffer and
 *              push it (change-detected + throttled) to any connected browser over a
 *              WebSocket,
 *   - input  : keydown/keyup from the browser -> canvas_button, returned to canvasd.
 * Serves one static page on http://localhost:$CANVAS_WEB_PORT (default 8080). Loopback
 * only — reach it from your Mac with `ssh -L 8080:localhost:8080 <box>`. Host-dev-only;
 * never compiled into the T113 image. Only host dep is zlib (frame compression); the
 * WebSocket itself is hand-rolled (see websocket.c). */
#define _GNU_SOURCE
#include "backend.h"
#include "websocket.h"
#include "canvas-proto.h"      /* CANVAS_BTN_* for key mapping */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <zlib.h>

/* Amazon Linux 2 / glibc 2.26 lacks the memfd_create() wrapper (added in glibc 2.27),
 * so call the syscall directly. (Kernel support since 3.17; this box runs 5.10.) */
#ifndef SYS_memfd_create
#  ifdef __NR_memfd_create
#    define SYS_memfd_create __NR_memfd_create
#  endif
#endif

#define MAXWS 4
#define MAXBUF 32
#define EVQ 64

static int      W = 800, H = 480;
static int      listen_fd = -1;
/* One browser connection. Frames are sent NON-BLOCKING and resumable: `out` holds the
 * WS frame currently in flight, drained across main-loop ticks (ws_pump). A connection
 * still finishing its previous frame is "busy" and simply gets the next frame DROPPED —
 * so a slow tunnel degrades video framerate instead of stalling the compositor (which is
 * single-threaded; a blocking write would freeze input + pacing until the link drained). */
static struct wsconn {
	int            fd;      /* -1 = free */
	unsigned char *out;     /* in-flight WS frame (header+payload), NULL until first use */
	size_t         cap;     /* allocated size of out */
	size_t         len;     /* bytes in the current frame; off==len => idle/ready */
	size_t         off;     /* bytes already written */
} ws[MAXWS];
static unsigned char *cur, *prev;          /* RGBA framebuffers (W*H*4) */
static unsigned char *zbuf;                /* deflate scratch — frames go compressed on the wire */
static uLong          zcap;                /* size of zbuf (compressBound of one frame) */
static long     last_send_ms;

static struct { int fd; void *ptr; size_t size; } reg[MAXBUF];  /* memfd registry */
static struct { uint32_t b; int32_t v; } evq[EVQ];
static int      evq_head, evq_tail;

static const char INDEX_HTML[] =
	"<!doctype html><meta charset=utf-8><title>canvas sim</title>\n"
	"<style>body{margin:0;min-height:100vh;background:#111;color:#888;font:13px monospace;\n"
	"display:flex;flex-direction:column;justify-content:center;align-items:center;gap:8px}\n"
	"canvas{image-rendering:pixelated;background:#000}</style>\n"
	"<canvas id=s width=800 height=480></canvas>\n"
	"<div>arrows = D-pad &nbsp; z or Space = A &nbsp; x=B a=X s=Y &nbsp; q=L w=R &nbsp; Enter=Start Shift=Select</div>\n"
	"<script>\n"
	"const x=document.getElementById('s').getContext('2d');\n"
	"const ws=new WebSocket('ws://'+location.host+'/ws');ws.binaryType='arraybuffer';\n"
	"async function draw(buf){\n"
	" const s=new Blob([buf]).stream().pipeThrough(new DecompressionStream('deflate'));\n"
	" const rgba=new Uint8ClampedArray(await new Response(s).arrayBuffer());\n"
	" x.putImageData(new ImageData(rgba,800,480),0,0);\n"
	"}\n"
	"let busy=false,pending=null;\n"          /* decode is async; only ever paint the newest frame */
	"ws.onmessage=async e=>{\n"
	" if(busy){pending=e.data;return;}\n"
	" busy=true;let d=e.data;\n"
	" do{await draw(d);d=pending;pending=null;}while(d);\n"
	" busy=false;\n"
	"};\n"
	"const K=t=>e=>{if(!e.repeat)ws.send(JSON.stringify({t,k:e.key}));e.preventDefault();};\n"
	"onkeydown=K('down');onkeyup=K('up');\n"
	"</script>\n";

static long now_ms(void)
{
	struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void *reg_lookup(int fd)
{
	for (int i = 0; i < MAXBUF; i++) if (reg[i].fd == fd) return reg[i].ptr;
	return NULL;
}

static void evq_push(uint32_t b, int32_t v)
{
	int nt = (evq_tail + 1) % EVQ;
	if (nt == evq_head) return;               /* full: drop */
	evq[evq_tail].b = b; evq[evq_tail].v = v; evq_tail = nt;
}

static int map_key(const char *k, uint32_t *btn)
{
	if      (!strcmp(k, "ArrowLeft"))  *btn = CANVAS_BTN_LEFT;
	else if (!strcmp(k, "ArrowRight")) *btn = CANVAS_BTN_RIGHT;
	else if (!strcmp(k, "ArrowUp"))    *btn = CANVAS_BTN_UP;
	else if (!strcmp(k, "ArrowDown"))  *btn = CANVAS_BTN_DOWN;
	else if (!strcmp(k, "z") || !strcmp(k, "Z") || !strcmp(k, " ")) *btn = CANVAS_BTN_A;  /* Space too (rollover-safe) */
	else if (!strcmp(k, "x") || !strcmp(k, "X")) *btn = CANVAS_BTN_B;
	else if (!strcmp(k, "a") || !strcmp(k, "A")) *btn = CANVAS_BTN_X;
	else if (!strcmp(k, "s") || !strcmp(k, "S")) *btn = CANVAS_BTN_Y;
	else if (!strcmp(k, "q") || !strcmp(k, "Q")) *btn = CANVAS_BTN_L;
	else if (!strcmp(k, "w") || !strcmp(k, "W")) *btn = CANVAS_BTN_R;
	else if (!strcmp(k, "Enter")) *btn = CANVAS_BTN_START;
	else if (!strcmp(k, "Shift")) *btn = CANVAS_BTN_SELECT;
	else return 0;
	return 1;
}

static int json_get(const char *s, const char *key, char *out, size_t cap)
{
	char pat[16]; snprintf(pat, sizeof pat, "\"%s\":\"", key);
	const char *p = strstr(s, pat); if (!p) return 0;
	p += strlen(pat);
	size_t i = 0; while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
	out[i] = 0; return 1;
}

static void ws_add(int fd)
{
	for (int i = 0; i < MAXWS; i++)
		if (ws[i].fd < 0) { ws[i].fd = fd; ws[i].len = ws[i].off = 0; return; }
	close(fd);                                 /* no room */
}

static void ws_close(int i)
{
	if (ws[i].fd >= 0) close(ws[i].fd);
	ws[i].fd = -1; ws[i].len = ws[i].off = 0;
}

/* Copy an RGBA frame into this connection's in-flight buffer as one WS binary frame.
 * Only call when idle (off==len) — a busy connection is still draining its previous one. */
static void ws_queue_frame(int i, const unsigned char *rgba, size_t bytes)
{
	size_t need = bytes + 10;                  /* + max WS header (10 bytes) */
	if (ws[i].cap < need) {
		unsigned char *p = realloc(ws[i].out, need);
		if (!p) return;                        /* OOM: skip this frame */
		ws[i].out = p; ws[i].cap = need;
	}
	unsigned char *h = ws[i].out; size_t hl;
	h[0] = 0x82;                               /* FIN + binary opcode */
	if (bytes < 126)        { h[1] = (unsigned char)bytes; hl = 2; }
	else if (bytes < 65536) { h[1] = 126; h[2] = bytes >> 8; h[3] = bytes & 0xff; hl = 4; }
	else { h[1] = 127; for (int k = 0; k < 8; k++) h[2+k] = (unsigned char)(bytes >> ((7-k)*8)); hl = 10; }
	memcpy(ws[i].out + hl, rgba, bytes);
	ws[i].len = hl + bytes; ws[i].off = 0;
}

/* Write as much of the in-flight frame as the socket will take right now WITHOUT blocking
 * (MSG_DONTWAIT); MSG_NOSIGNAL turns a dead-peer write into EPIPE instead of SIGPIPE.
 * Returns 0 (ok — maybe more to send next tick) or -1 (dead: caller closes). */
static int ws_pump(int i)
{
	while (ws[i].off < ws[i].len) {
		ssize_t w = send(ws[i].fd, ws[i].out + ws[i].off, ws[i].len - ws[i].off,
				 MSG_DONTWAIT | MSG_NOSIGNAL);
		if (w > 0) { ws[i].off += (size_t)w; continue; }
		if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;  /* socket full: later */
		return -1;                             /* real error / 0-length: drop the client */
	}
	return 0;
}

/* Deflate the current RGBA framebuffer into zbuf (zlib format; the browser inflates it with
 * a native DecompressionStream). Level 1 = fast; a flat 2D frame shrinks ~5-15x — the whole
 * point, since raw 1.5MB frames don't fit through an ssh tunnel at speed. 0 on failure. */
static size_t compress_cur(void)
{
	uLong zlen = zcap;
	if (compress2(zbuf, &zlen, cur, (uLong)W * H * 4, 1) != Z_OK) return 0;
	return (size_t)zlen;
}

/* accept one HTTP connection: serve the page, or upgrade it to a WebSocket. */
static void accept_http(void)
{
	int c = accept(listen_fd, NULL, NULL);
	if (c < 0) return;
	char req[2048]; ssize_t n = read(c, req, sizeof req - 1);
	if (n <= 0) { close(c); return; }
	req[n] = 0;
	if (strstr(req, "Upgrade:") || strstr(req, "upgrade:")) {
		if (ws_handshake(c, req)) {            /* handshake is tiny; blocking write is fine */
			/* Post-handshake: flip to non-blocking so frame sends never stall the loop. */
			int fl = fcntl(c, F_GETFL, 0); if (fl >= 0) fcntl(c, F_SETFL, fl | O_NONBLOCK);
			ws_add(c);
			size_t zl = compress_cur();       /* paint the current (compressed) frame immediately */
			for (int i = 0; i < MAXWS; i++)
				if (ws[i].fd == c) { if (zl) ws_queue_frame(i, zbuf, zl); ws_pump(i); break; }
			return;
		}
	}
	char hdr[128];
	int hl = snprintf(hdr, sizeof hdr,
		"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n"
		"Connection: close\r\n\r\n", sizeof(INDEX_HTML) - 1);
	if (write(c, hdr, (size_t)hl) > 0) { if (write(c, INDEX_HTML, sizeof(INDEX_HTML) - 1) < 0) {} }
	close(c);
}

/* ---- backend interface ------------------------------------------------------ */

int backend_init(uint32_t *w, uint32_t *h)
{
	for (int i = 0; i < MAXWS; i++) { ws[i].fd = -1; ws[i].out = NULL; ws[i].cap = ws[i].len = ws[i].off = 0; }
	for (int i = 0; i < MAXBUF; i++) reg[i].fd = -1;
	*w = (uint32_t)W; *h = (uint32_t)H;

	cur  = malloc((size_t)W * H * 4);
	prev = malloc((size_t)W * H * 4);
	if (!cur || !prev) return -1;
	memset(cur, 0x20, (size_t)W * H * 4);      /* "no signal" gray until a client draws */
	memset(prev, 0x00, (size_t)W * H * 4);     /* differs from cur -> first present sends */
	zcap = compressBound((uLong)W * H * 4);
	zbuf = malloc(zcap);
	if (!zbuf) return -1;

	const char *ps = getenv("CANVAS_WEB_PORT");
	int port = ps ? atoi(ps) : 8080;
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in sa = { 0 };
	sa.sin_family = AF_INET; sa.sin_port = htons((uint16_t)port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* localhost only; reach via ssh -L */
	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(listen_fd, 8) < 0) {
		perror("backend_web: bind"); return -1;
	}
	fprintf(stderr, "backend_web: http+ws on http://localhost:%d  (ssh -L %d:localhost:%d <box>)\n",
		port, port, port);
	return 0;
}

int backend_alloc_buffer(uint32_t *stride, uint32_t *size)
{
	*stride = (uint32_t)W * 4;
	*size   = *stride * (uint32_t)H;
	int fd = (int)syscall(SYS_memfd_create, "canvasbuf", 0u);
	if (fd < 0) return -1;
	if (ftruncate(fd, *size) < 0) { close(fd); return -1; }
	void *p = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) { close(fd); return -1; }
	for (int i = 0; i < MAXBUF; i++)
		if (reg[i].fd < 0) { reg[i].fd = fd; reg[i].ptr = p; reg[i].size = *size; return fd; }
	munmap(p, *size); close(fd); return -1;    /* registry full */
}

void backend_present(int fg_fd)
{
	if (fg_fd >= 0) {
		uint32_t *src = reg_lookup(fg_fd);
		if (src)
			for (size_t i = 0; i < (size_t)W * H; i++) {
				uint32_t v = src[i];             /* 0x00RRGGBB */
				cur[i*4+0] = (v >> 16) & 0xff;
				cur[i*4+1] = (v >> 8)  & 0xff;
				cur[i*4+2] =  v        & 0xff;
				cur[i*4+3] = 0xff;
			}
	}
	long now = now_ms();
	if (memcmp(cur, prev, (size_t)W * H * 4) != 0 && now - last_send_ms >= 50) {  /* ~20fps cap */
		size_t zl = compress_cur();                 /* deflate once, hand the same bytes to all */
		if (zl)
			for (int i = 0; i < MAXWS; i++) {
				if (ws[i].fd < 0) continue;
				if (ws[i].off < ws[i].len) continue;    /* still draining prior frame -> DROP */
				ws_queue_frame(i, zbuf, zl);
			}
		memcpy(prev, cur, (size_t)W * H * 4);
		last_send_ms = now;
	}
	/* Every tick: push whatever more the socket will take, without blocking. Failures
	 * (peer gone) close the connection here so the poll loop stays healthy. */
	for (int i = 0; i < MAXWS; i++)
		if (ws[i].fd >= 0 && ws[i].off < ws[i].len && ws_pump(i) < 0) ws_close(i);
}

int backend_poll_input(uint32_t *button, int32_t *value)
{
	if (evq_head != evq_tail) {                /* drain the queue without re-polling */
		*button = evq[evq_head].b; *value = evq[evq_head].v;
		evq_head = (evq_head + 1) % EVQ; return 1;
	}
	struct pollfd p[1 + MAXWS]; int idx[1 + MAXWS]; int n = 0;
	p[n].fd = listen_fd; p[n].events = POLLIN; idx[n] = -1; n++;
	for (int i = 0; i < MAXWS; i++)
		if (ws[i].fd >= 0) { p[n].fd = ws[i].fd; p[n].events = POLLIN; idx[n] = i; n++; }
	if (poll(p, n, 0) <= 0) return 0;
	if (p[0].revents & POLLIN) accept_http();
	for (int k = 1; k < n; k++) {
		if (!(p[k].revents & POLLIN)) continue;
		char b[512]; int r = ws_recv(ws[idx[k]].fd, b, sizeof b);
		if (r < 0) { ws_close(idx[k]); }
		else if (r > 0) {
			char t[8], kk[24]; uint32_t bn;
			if (json_get(b, "k", kk, sizeof kk) && json_get(b, "t", t, sizeof t) && map_key(kk, &bn))
				evq_push(bn, strcmp(t, "down") == 0 ? 1 : 0);
		}
	}
	if (evq_head != evq_tail) {
		*button = evq[evq_head].b; *value = evq[evq_head].v;
		evq_head = (evq_head + 1) % EVQ; return 1;
	}
	return 0;
}

void backend_fini(void)
{
	for (int i = 0; i < MAXWS; i++) { if (ws[i].fd >= 0) close(ws[i].fd); free(ws[i].out); }
	if (listen_fd >= 0) close(listen_fd);
	free(cur); free(prev); free(zbuf);
}
