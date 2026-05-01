/*
 * dbg_client.c — TCP client for the sim-core debug stub
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

void dbg_send(struct dbg_client *c, const char *json)
{
    write(c->fd, json, strlen(json));
    write(c->fd, "\n", 1);
}

char *dbg_recv(struct dbg_client *c)
{
    static char line[8192];
    while (1) {
        /* Check if we already have a complete line */
        for (int i = 0; i < c->buf_len; i++) {
            if (c->buf[i] == '\n') {
                memcpy(line, c->buf, i);
                line[i] = '\0';
                int remaining = c->buf_len - i - 1;
                memmove(c->buf, c->buf + i + 1, remaining);
                c->buf_len = remaining;
                return line;
            }
        }
        /* Read more data */
        int n = read(c->fd, c->buf + c->buf_len, sizeof(c->buf) - c->buf_len - 1);
        if (n <= 0) return NULL;
        c->buf_len += n;
    }
}

int dbg_data_ready(struct dbg_client *c)
{
    /* Check buffered data first */
    for (int i = 0; i < c->buf_len; i++)
        if (c->buf[i] == '\n') return 1;
    /* Check socket */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(c->fd, &fds);
    struct timeval tv = {0, 0};
    return select(c->fd + 1, &fds, NULL, NULL, &tv) > 0;
}

void dbg_close(struct dbg_client *c)
{
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
}
