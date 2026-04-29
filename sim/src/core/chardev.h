/*
 * chardev.h — Named character device channels
 *
 * Each chardev is a TCP socket that a device can write to / read from.
 * The launcher decides which devices get external connections.
 */
#ifndef CHARDEV_H
#define CHARDEV_H

#include <stdint.h>

#define MAX_CHARDEVS 8

struct chardev {
    char name[32];    /* device label from DTS, e.g. "usart2" */
    int port;         /* TCP port to listen on */
    int srv_fd;       /* listening socket */
    int client_fd;    /* connected client, -1 if none */
};

struct chardev_table {
    struct chardev devs[MAX_CHARDEVS];
    int count;
};

void chardev_table_init(struct chardev_table *t);

/* Parse "--chardev name=port" and add to table. Returns 0 on success. */
int chardev_add(struct chardev_table *t, const char *spec);

/* Start listening on all chardev ports */
void chardev_listen_all(struct chardev_table *t);

/* Find a chardev by name. Returns NULL if not found. */
struct chardev *chardev_find(struct chardev_table *t, const char *name);

/* Try to accept a client (non-blocking). Call periodically. */
void chardev_try_accept(struct chardev *cd);

/* Write a byte to the chardev's client. No-op if no client. */
void chardev_write(struct chardev *cd, uint8_t byte);

/* Write a buffer to the chardev's client. No-op if no client. */
void chardev_write_buf(struct chardev *cd, const uint8_t *data, int len);

/* Non-blocking read from chardev client. Returns bytes read, 0 if nothing, -1 on disconnect. */
int chardev_read_nonblock(struct chardev *cd, uint8_t *buf, int maxlen);

#endif
