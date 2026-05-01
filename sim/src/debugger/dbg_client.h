/*
 * dbg_client.h — TCP client for the sim-core debug stub
 */
#ifndef DBG_CLIENT_H
#define DBG_CLIENT_H

#include <stdint.h>

struct dbg_client {
    int fd;
    char buf[8192];
    int buf_len;
};

/* Connect to debug stub. Returns 0 on success. */
int dbg_connect(struct dbg_client *c, const char *host, int port);

/* Send a JSON command (appends newline). */
void dbg_send(struct dbg_client *c, const char *json);

/* Receive one newline-terminated JSON response. Blocks. Returns static buffer. */
char *dbg_recv(struct dbg_client *c);

/* Check if data is available (non-blocking). */
int dbg_data_ready(struct dbg_client *c);

/* Close connection. */
void dbg_close(struct dbg_client *c);

#endif
