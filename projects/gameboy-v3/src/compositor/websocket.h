/* websocket.h — a minimal WebSocket for the web backend (host-dev-only). No deps:
 * inline SHA-1 + base64 for the RFC 6455 handshake, plus binary-send / frame-recv.
 * Just enough to push raw RGBA frames to a browser and read tiny key messages back. */
#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#include <stddef.h>

/* Given the raw HTTP request text, if it's a WS upgrade, send the 101 response and
 * return 1 (the fd is now a WebSocket); else return 0 (caller serves it as HTTP). */
int ws_handshake(int fd, const char *request);

/* Send one unmasked binary frame. 0 = ok, -1 = error (drop the client). */
int ws_send_binary(int fd, const void *data, size_t len);

/* Read one frame from fd (call after poll() says readable). For a text/binary frame,
 * null-terminates buf and returns its length (>=0). Returns -1 on close/error, 0 for
 * control frames (ping/pong) or an oversized frame that was drained. */
int ws_recv(int fd, char *buf, size_t cap);

#endif /* WEBSOCKET_H */
