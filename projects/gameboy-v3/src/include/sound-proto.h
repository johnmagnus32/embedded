/* sound-proto.h — the gameboy-v3 soundd <-> client wire protocol (the audio twin of
 * canvas-proto.h). Same transport as the compositor: one SEQPACKET unix socket per client,
 * fixed-size `struct sound_msg`, and the ONE message that ships a kernel object (SOUND_RING,
 * a shared-memory PCM ring memfd) passes it out-of-band via SCM_RIGHTS.
 *
 * Difference from canvas: canvas hands each client N discrete swap buffers gated by RELEASE;
 * soundd hands each client ONE contiguous PCM RING and tracks monotonic cursors —
 *   client:  writes interleaved int16 stereo into the ring, sends SOUND_SUBMIT{written}
 *            (total frames ever written).
 *   soundd:  mixes ALL clients' rings to the device, sends SOUND_CONSUMED{consumed}
 *            (total frames it has drained) so the client knows how much ring space is free.
 * PCM travels ONLY through the shared ring; the socket carries just control + cursors.
 */
#ifndef SOUND_PROTO_H
#define SOUND_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define SOUND_SOCK_PATH "/run/sound/session.sock"
#define SOUND_RATE      44100     /* fixed format (matches the engine mixer + gen_sfx.py) */
#define SOUND_CH        2         /* interleaved stereo int16 */

/* Socket path with a SOUND_SOCK env override — lets the host simulator use a user-writable
 * path (e.g. /tmp) instead of /run/sound. Used by soundd + libsound. */
static inline const char *sound_sock_path(void)
{
	const char *p = getenv("SOUND_SOCK");
	return (p && *p) ? p : SOUND_SOCK_PATH;
}

enum sound_role {
	SOUND_ROLE_GAME = 0,      /* a game/launcher producing audio */
};

enum sound_op {
	SOUND_HELLO,      /* c->s: I am a client of role u.hello.role                         */
	SOUND_WELCOME,    /* s->c: audio format + ring capacity (u.welcome)                    */
	SOUND_RING,       /* s->c: the shared PCM ring (u.ring.size bytes) + a memfd fd        */
	SOUND_SUBMIT,     /* c->s: I have now written u.submit.written total frames            */
	SOUND_CONSUMED,   /* s->c: I have now drained u.consumed.consumed total frames         */
	SOUND_QUIT,       /* c->s: clean teardown (soundd also reaps on socket close)          */
};

struct sound_msg {
	uint32_t op;
	union {
		struct { uint32_t role; }                                   hello;
		struct { uint32_t rate, channels, ring_frames, ring_bytes; } welcome;
		struct { uint32_t size; }                                   ring;
		struct { uint64_t written; }                                submit;
		struct { uint64_t consumed; }                               consumed;
	} u;
};

/* ---- wire helpers: fixed-size message + optional single fd via SCM_RIGHTS ----
 * Byte-for-byte the canvas_send/canvas_recv mechanic (SEQPACKET, one optional fd). */

static inline int sound_send(int sock, const struct sound_msg *m, int passfd)
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

static inline int sound_recv(int sock, struct sound_msg *m, int *outfd)
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

#endif /* SOUND_PROTO_H */
