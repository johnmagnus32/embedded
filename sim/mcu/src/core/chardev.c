/*
 * chardev.c — Named character device channels over TCP
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "chardev.h"

void chardev_table_init(struct chardev_table *t)
{
    t->count = 0;
}

int chardev_add(struct chardev_table *t, const char *spec)
{
    /* spec = "name=port" */
    if (t->count >= MAX_CHARDEVS) return -1;
    const char *eq = strchr(spec, '=');
    if (!eq) return -1;

    struct chardev *cd = &t->devs[t->count++];
    int namelen = (int)(eq - spec);
    if (namelen > 31) namelen = 31;
    memcpy(cd->name, spec, namelen);
    cd->name[namelen] = '\0';
    cd->port = atoi(eq + 1);
    cd->srv_fd = -1;
    cd->client_fd = -1;
    return 0;
}

void chardev_listen_all(struct chardev_table *t)
{
    for (int i = 0; i < t->count; i++) {
        struct chardev *cd = &t->devs[i];
        cd->srv_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(cd->srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(cd->port),
            .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
        };
        if (bind(cd->srv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "[chardev] Failed to bind %s on port %d\n", cd->name, cd->port);
            close(cd->srv_fd);
            cd->srv_fd = -1;
            continue;
        }
        listen(cd->srv_fd, 1);
        fprintf(stderr, "[chardev] %s listening on port %d\n", cd->name, cd->port);
    }
}

struct chardev *chardev_find(struct chardev_table *t, const char *name)
{
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->devs[i].name, name) == 0)
            return &t->devs[i];
    return NULL;
}

void chardev_try_accept(struct chardev *cd)
{
    if (!cd || cd->srv_fd < 0 || cd->client_fd >= 0) return;
    int flags = fcntl(cd->srv_fd, F_GETFL, 0);
    fcntl(cd->srv_fd, F_SETFL, flags | O_NONBLOCK);
    cd->client_fd = accept(cd->srv_fd, NULL, NULL);
    fcntl(cd->srv_fd, F_SETFL, flags);
    /* Set client socket non-blocking so writes never stall the emulator */
    if (cd->client_fd >= 0) {
        int cflags = fcntl(cd->client_fd, F_GETFL, 0);
        fcntl(cd->client_fd, F_SETFL, cflags | O_NONBLOCK);
    }
}

void chardev_write(struct chardev *cd, uint8_t byte)
{
    if (!cd) return;
    if (cd->wbuf_len < (int)sizeof(cd->wbuf))
        cd->wbuf[cd->wbuf_len++] = byte;
}

void chardev_write_buf(struct chardev *cd, const uint8_t *data, int len)
{
    if (!cd) return;
    int space = (int)sizeof(cd->wbuf) - cd->wbuf_len;
    if (len > space) len = space;
    if (len > 0) {
        memcpy(cd->wbuf + cd->wbuf_len, data, len);
        cd->wbuf_len += len;
    }
}

void chardev_flush(struct chardev *cd)
{
    if (!cd || cd->wbuf_len == 0) return;
    if (cd->client_fd < 0) chardev_try_accept(cd);
    if (cd->client_fd >= 0) {
        int sent = 0;
        while (sent < cd->wbuf_len) {
            int n = write(cd->client_fd, cd->wbuf + sent, cd->wbuf_len - sent);
            if (n > 0) { sent += n; continue; }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;  /* socket full — keep remaining data for next flush */
            cd->client_fd = -1;
            break;
        }
        /* Shift unsent data to front of buffer */
        if (sent > 0 && sent < cd->wbuf_len) {
            memmove(cd->wbuf, cd->wbuf + sent, cd->wbuf_len - sent);
        }
        cd->wbuf_len -= sent;
    }
}

void chardev_flush_all(struct chardev_table *t)
{
    if (!t) return;
    for (int i = 0; i < t->count; i++)
        chardev_flush(&t->devs[i]);
}

int chardev_read_nonblock(struct chardev *cd, uint8_t *buf, int maxlen)
{
    if (!cd) return 0;
    if (cd->client_fd < 0) chardev_try_accept(cd);
    if (cd->client_fd < 0) return 0;
    fd_set fds; struct timeval tv = {0, 0};
    FD_ZERO(&fds); FD_SET(cd->client_fd, &fds);
    if (select(cd->client_fd + 1, &fds, NULL, NULL, &tv) <= 0) return 0;
    int n = read(cd->client_fd, buf, maxlen);
    if (n <= 0) { cd->client_fd = -1; return -1; }
    return n;
}

void chardev_shutdown_all(struct chardev_table *t)
{
    if (!t) return;
    for (int i = 0; i < t->count; i++) {
        struct chardev *cd = &t->devs[i];
        if (cd->client_fd >= 0) {
            /* Set blocking so final write completes */
            int flags = fcntl(cd->client_fd, F_GETFL, 0);
            fcntl(cd->client_fd, F_SETFL, flags & ~O_NONBLOCK);
            /* Flush remaining data */
            if (cd->wbuf_len > 0) {
                write(cd->client_fd, cd->wbuf, cd->wbuf_len);
                cd->wbuf_len = 0;
            }
            shutdown(cd->client_fd, SHUT_WR);
        }
    }
}
