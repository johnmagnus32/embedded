/* appletd-proto.h — the appletd control protocol. appletd is the application/applet
 * manager (the Switch am+pm analog): it owns the PROCESS LIFECYCLE of the app layer
 * — spawning/reaping the home menu and games — which the compositor (canvasd)
 * deliberately does NOT do. This is a SEPARATE channel from the canvas display
 * protocol (canvas-proto.h): a client DRAWS via canvasd, but asks appletd to RUN
 * things. Transport: a SEQPACKET unix socket, fixed-size messages, no fds.
 *
 * PHASE 1: just LAUNCH_APP. PHASE 2 adds suspend/resume + a canvasd control link
 * for the HOME overlay, plus library-applet requests (keyboard/dialog).
 */
#ifndef APPLETD_PROTO_H
#define APPLETD_PROTO_H

#include <stdint.h>
#include <sys/socket.h>

#define APPLETD_SOCK_PATH "/run/canvas/appletd.sock"

enum appletd_op {
	APPLETD_LAUNCH_APP,   /* c->appletd: run u.launch.path as the Application */
	APPLETD_OK,           /* appletd->c: accepted                            */
	APPLETD_ERR,          /* appletd->c: rejected (spawn failed)             */
};

struct appletd_msg {
	uint32_t op;
	union {
		struct { char path[224]; } launch;
	} u;
};

static inline int appletd_send(int s, const struct appletd_msg *m)
{
	return send(s, m, sizeof(*m), 0) == (ssize_t)sizeof(*m) ? 0 : -1;
}
static inline int appletd_recv(int s, struct appletd_msg *m)
{
	return recv(s, m, sizeof(*m), 0) == (ssize_t)sizeof(*m) ? 0 : -1;
}

#endif /* APPLETD_PROTO_H */
