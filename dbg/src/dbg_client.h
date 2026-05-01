/*
 * dbg_client.h — GDB RSP client for sim-dbg
 */
#ifndef DBG_CLIENT_H
#define DBG_CLIENT_H

#include <stdint.h>

struct dbg_client {
    int fd;
    char buf[8192];
    int buf_len;
};

int dbg_connect(struct dbg_client *c, const char *host, int port);
void dbg_send(struct dbg_client *c, const char *data);
char *dbg_recv(struct dbg_client *c);
int dbg_data_ready(struct dbg_client *c);
void dbg_send_halt(struct dbg_client *c);
void dbg_close(struct dbg_client *c);

#endif
