/*
 * dbg_client.c — GDB RSP client for sim-dbg
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dbg_client.h"

int dbg_connect(struct dbg_client *c, const char *host, int port)
{
    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->fd < 0) return -1;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(c->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(c->fd);
        c->fd = -1;
        return -1;
    }
    c->buf_len = 0;
    return 0;
}

void dbg_send(struct dbg_client *c, const char *data)
{
    uint8_t sum = 0;
    for (const char *p = data; *p; p++) sum += (uint8_t)*p;
    char buf[16384];
    int n = snprintf(buf, sizeof(buf), "$%s#%02x", data, sum);
    write(c->fd, buf, n);
}

char *dbg_recv(struct dbg_client *c)
{
    static char line[8192];
    char ch;

    /* Skip until $ (consume + ACK bytes) */
    do {
        if (read(c->fd, &ch, 1) <= 0) return NULL;
    } while (ch != '$');

    /* Read until # */
    int len = 0;
    while (len < (int)sizeof(line) - 1) {
        if (read(c->fd, &ch, 1) <= 0) return NULL;
        if (ch == '#') break;
        line[len++] = ch;
    }
    line[len] = '\0';

    /* Consume 2-byte checksum */
    char ck[2];
    read(c->fd, ck, 2);

    /* Send ACK */
    write(c->fd, "+", 1);

    return line;
}

int dbg_data_ready(struct dbg_client *c)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(c->fd, &fds);
    struct timeval tv = {0, 0};
    return select(c->fd + 1, &fds, NULL, NULL, &tv) > 0;
}

void dbg_send_halt(struct dbg_client *c)
{
    /* Ctrl+C: raw 0x03 byte, not a packet */
    char ctrl_c = 0x03;
    write(c->fd, &ctrl_c, 1);
}

void dbg_close(struct dbg_client *c)
{
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
}
