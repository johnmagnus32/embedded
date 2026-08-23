/* websocket.c — a minimal WebSocket (RFC 6455) server for the web sim backend. Host-dev
 * only; never built into the T113 image. It handles exactly what the simulator needs:
 * the opening handshake, sending unmasked binary frames (the compressed video frames), and
 * reading one masked frame from the browser (a key event). It deliberately does NOT do
 * fragmentation, ping/pong, TLS, or a clean close handshake — see the note in websocket.h.
 *
 * SHA-1 and base64 are inlined below because the handshake needs them and we don't want to
 * pull in a crypto dependency for one hash. */
#define _GNU_SOURCE
#include "websocket.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* RFC 6455 frame bits (first two header bytes). */
#define WS_FIN         0x80          /* byte 0: final fragment (we only send whole frames) */
#define WS_OP_TEXT     0x1           /* byte 0 low nibble: opcode */
#define WS_OP_BINARY   0x2
#define WS_OP_CLOSE    0x8
#define WS_OP_PING     0x9           /* (received pings are ignored) */
#define WS_OP_PONG     0xA
#define WS_MASK_BIT    0x80          /* byte 1 high bit: payload is masked (client->server always) */
#define WS_LEN_16      126           /* byte 1 low 7 bits == 126: real length is the next 2 bytes */
#define WS_LEN_64      127           /* == 127: real length is the next 8 bytes */

/* The fixed GUID a server appends to the client key before hashing (RFC 6455 §1.3). */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* ---- SHA-1 (one-shot, public-domain style) — only used to derive the accept key ------- */

/* Mix one 64-byte (512-bit) block into the running state s[0..4]. Textbook SHA-1. */
static void sha1_block(uint32_t s[5], const unsigned char block[64])
{
	uint32_t w[80];
	for (int i = 0; i < 16; i++)                 /* load 16 big-endian 32-bit words ... */
		w[i] = (block[i*4] << 24) | (block[i*4+1] << 16) | (block[i*4+2] << 8) | block[i*4+3];
	for (int i = 16; i < 80; i++) {              /* ... extend to 80 (rotate-left-1 of an xor) */
		uint32_t x = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
		w[i] = (x << 1) | (x >> 31);
	}

	uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
	for (int i = 0; i < 80; i++) {
		uint32_t f, k;                           /* round function + constant (change every 20) */
		if      (i < 20) { f = (b & c) | (~b & d);          k = 0x5A827999; }
		else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
		else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
		uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
		e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
	}
	s[0] += a; s[1] += b; s[2] += c; s[3] += d; s[4] += e;
}

/* SHA-1 of msg[0..len) -> out[20]. */
static void sha1(const unsigned char *msg, size_t len, unsigned char out[20])
{
	uint32_t s[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	size_t full = len / 64, rem = len % 64;
	for (size_t i = 0; i < full; i++) sha1_block(s, msg + i*64);

	/* Final block: the leftover bytes, a 0x80 terminator, then the message bit-length in the
	 * last 8 bytes. If the leftover + terminator leave no room for the length, spill to a
	 * second block. */
	unsigned char block[64] = { 0 };
	memcpy(block, msg + full*64, rem);
	block[rem] = 0x80;
	if (rem >= 56) { sha1_block(s, block); memset(block, 0, 64); }
	uint64_t bits = (uint64_t)len * 8;
	for (int i = 0; i < 8; i++) block[63 - i] = (unsigned char)(bits >> (i*8));
	sha1_block(s, block);

	for (int i = 0; i < 5; i++) {                /* state words -> big-endian output bytes */
		out[i*4+0] = s[i] >> 24; out[i*4+1] = s[i] >> 16;
		out[i*4+2] = s[i] >> 8;  out[i*4+3] = s[i];
	}
}

/* Base64-encode in[0..n) into out (NUL-terminated). Sized for the 20-byte digest. */
static void b64(const unsigned char *in, size_t n, char *out)
{
	static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i, o = 0;
	for (i = 0; i + 2 < n; i += 3) {             /* whole 3-byte groups -> 4 chars */
		uint32_t v = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
		out[o++] = T[(v >> 18) & 63]; out[o++] = T[(v >> 12) & 63];
		out[o++] = T[(v >>  6) & 63]; out[o++] = T[ v        & 63];
	}
	if (i < n) {                                 /* 1 or 2 trailing bytes -> '=' padding */
		int have_two = (i + 1 < n);
		uint32_t v = in[i] << 16;
		if (have_two) v |= in[i+1] << 8;
		out[o++] = T[(v >> 18) & 63];
		out[o++] = T[(v >> 12) & 63];
		out[o++] = have_two ? T[(v >> 6) & 63] : '=';
		out[o++] = '=';
	}
	out[o] = 0;
}

/* ---- transfer helpers: loop until the whole buffer moves (-1 on EOF/error) ------------ */

static int full_read(int fd, void *buf, size_t n)
{
	size_t done = 0;
	while (done < n) {
		ssize_t r = read(fd, (char *)buf + done, n - done);
		if (r <= 0) return -1;
		done += (size_t)r;
	}
	return 0;
}

static int full_write(int fd, const void *buf, size_t n)
{
	size_t done = 0;
	while (done < n) {
		ssize_t w = write(fd, (const char *)buf + done, n - done);
		if (w <= 0) return -1;
		done += (size_t)w;
	}
	return 0;
}

/* ---- API -------------------------------------------------------------------------------
 * Complete the opening handshake on a freshly-accepted HTTP connection whose request is in
 * `req`. The reply echoes base64(SHA1(client-key + GUID)). Returns 1 on success, 0 if the
 * request isn't a valid upgrade. */
int ws_handshake(int fd, const char *req)
{
	const char *p = strcasestr(req, "sec-websocket-key:");
	if (!p) return 0;
	p += strlen("sec-websocket-key:");
	while (*p == ' ') p++;

	/* Build "<client-key><GUID>" and hash it. */
	char keyed[128];
	int n = 0;
	while (*p && *p != '\r' && *p != '\n' && n < (int)(sizeof(keyed) - sizeof(WS_GUID)))
		keyed[n++] = *p++;
	memcpy(keyed + n, WS_GUID, sizeof(WS_GUID));   /* copies the 36 chars + their NUL */

	unsigned char digest[20];
	sha1((unsigned char *)keyed, strlen(keyed), digest);
	char accept[40];
	b64(digest, 20, accept);

	char resp[256];
	int len = snprintf(resp, sizeof resp,
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n\r\n", accept);
	return full_write(fd, resp, (size_t)len) == 0 ? 1 : 0;
}

/* Send data[0..len) as one unmasked binary frame (server->client frames are never masked).
 * The header is 2, 4, or 10 bytes depending on the payload size. Returns 0 / -1. */
int ws_send_binary(int fd, const void *data, size_t len)
{
	unsigned char hdr[10];
	size_t hlen;

	hdr[0] = WS_FIN | WS_OP_BINARY;
	if (len < WS_LEN_16) {                       /* 0..125: length is byte 1 itself */
		hdr[1] = (unsigned char)len;
		hlen = 2;
	} else if (len < 65536) {                    /* up to 64 KiB: 16-bit extended length */
		hdr[1] = WS_LEN_16;
		hdr[2] = (unsigned char)(len >> 8);
		hdr[3] = (unsigned char)(len);
		hlen = 4;
	} else {                                     /* larger: 64-bit extended length */
		hdr[1] = WS_LEN_64;
		for (int i = 0; i < 8; i++) hdr[2+i] = (unsigned char)(len >> ((7 - i) * 8));
		hlen = 10;
	}

	if (full_write(fd, hdr, hlen) < 0) return -1;
	return full_write(fd, data, len);
}

/* Read one frame into buf (up to cap-1 bytes, NUL-terminated). Client frames are always
 * masked, so we unmask in place. Returns the payload length for a text/binary message, 0
 * for a frame we ignore (ping/pong, or one too big for buf), and -1 on close/error. */
int ws_recv(int fd, char *buf, size_t cap)
{
	unsigned char h[2];
	if (full_read(fd, h, 2) < 0) return -1;
	int      opcode = h[0] & 0x0f;
	int      masked = h[1] & WS_MASK_BIT;
	uint64_t len    = h[1] & 0x7f;

	if (len == WS_LEN_16) {                      /* 16-bit extended length follows */
		unsigned char ext[2];
		if (full_read(fd, ext, 2) < 0) return -1;
		len = ((uint64_t)ext[0] << 8) | ext[1];
	} else if (len == WS_LEN_64) {               /* 64-bit extended length follows */
		unsigned char ext[8];
		if (full_read(fd, ext, 8) < 0) return -1;
		len = 0;
		for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
	}

	unsigned char mask[4] = { 0 };
	if (masked && full_read(fd, mask, 4) < 0) return -1;

	if (opcode == WS_OP_CLOSE) return -1;        /* client hung up */

	if (len >= cap) {                            /* too big for buf: drain it and ignore */
		unsigned char scratch[256];
		while (len > 0) {
			size_t chunk = len < sizeof scratch ? (size_t)len : sizeof scratch;
			if (full_read(fd, scratch, chunk) < 0) return -1;
			len -= chunk;
		}
		return 0;
	}

	if (full_read(fd, buf, (size_t)len) < 0) return -1;
	if (masked)
		for (uint64_t i = 0; i < len; i++) buf[i] ^= mask[i % 4];

	if (opcode == WS_OP_TEXT || opcode == WS_OP_BINARY) {
		buf[len] = 0;
		return (int)len;
	}
	return 0;                                     /* ping/pong/continuation: ignore */
}
