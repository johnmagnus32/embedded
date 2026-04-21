/*
 * msgq.c — Fixed-size message queue
 */

#include "msgq.h"
#include "sched.h"
#include <stdint.h>

/* Use same irq_lock pattern as sync.c */
static inline uint32_t irq_lock(void)
{
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));
    return key;
}

static inline void irq_unlock(uint32_t key)
{
    __asm volatile("msr primask, %0" :: "r"(key));
}

static void memcpy_small(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
}

void msgq_put(struct msgq *q, const void *msg)
{
    while (1) {
        uint32_t key = irq_lock();
        if (q->count < MSGQ_MAX_MSGS) {
            memcpy_small(q->buf[q->head], msg, q->msg_size);
            q->head = (q->head + 1) % MSGQ_MAX_MSGS;
            q->count++;
            irq_unlock(key);
            return;
        }
        irq_unlock(key);
        sched_yield();  /* full — wait */
    }
}

void msgq_get(struct msgq *q, void *msg)
{
    while (1) {
        uint32_t key = irq_lock();
        if (q->count > 0) {
            memcpy_small(msg, q->buf[q->tail], q->msg_size);
            q->tail = (q->tail + 1) % MSGQ_MAX_MSGS;
            q->count--;
            irq_unlock(key);
            return;
        }
        irq_unlock(key);
        sched_yield();  /* empty — wait */
    }
}

int msgq_try_get(struct msgq *q, void *msg)
{
    uint32_t key = irq_lock();
    if (q->count > 0) {
        memcpy_small(msg, q->buf[q->tail], q->msg_size);
        q->tail = (q->tail + 1) % MSGQ_MAX_MSGS;
        q->count--;
        irq_unlock(key);
        return 0;
    }
    irq_unlock(key);
    return -1;
}
