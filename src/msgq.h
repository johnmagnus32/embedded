/*
 * msgq.h — Fixed-size message queue
 *
 * Like Zephyr's k_msgq — a FIFO of fixed-size messages.
 * put() copies a message in, get() copies one out.
 * Blocks (yields) if full/empty.
 *
 * Used for passing data between tasks without shared variables.
 */

#ifndef MSGQ_H
#define MSGQ_H

#include <stdint.h>
#include <stddef.h>

#define MSGQ_MAX_MSGS 8
#define MSGQ_MAX_MSG_SIZE 32

struct msgq {
    uint8_t buf[MSGQ_MAX_MSGS][MSGQ_MAX_MSG_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
    uint8_t msg_size;
};

#define MSGQ_INIT(msize) { .head = 0, .tail = 0, .count = 0, .msg_size = (msize) }

/* Put a message (blocks if full). */
void msgq_put(struct msgq *q, const void *msg);

/* Get a message (blocks if empty). */
void msgq_get(struct msgq *q, void *msg);

/* Try to get without blocking. Returns 0 on success, -1 if empty. */
int msgq_try_get(struct msgq *q, void *msg);

#endif
