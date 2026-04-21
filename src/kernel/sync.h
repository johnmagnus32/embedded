/*
 * sync.h — Mutex and semaphore with wait queues
 *
 * sem_take() now BLOCKS (task removed from ready queue) instead of
 * spin-yielding. The task consumes zero CPU while waiting.
 * sem_give() wakes one blocked task.
 */

#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "sched.h"

/* --- Mutex --- */

struct mutex {
    volatile int8_t owner;
    struct wait_queue wq;
};

#define MUTEX_INIT { .owner = -1, .wq = WAIT_QUEUE_INIT }

void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

/* --- Semaphore --- */

struct semaphore {
    volatile int16_t count;
    struct wait_queue wq;
};

#define SEM_INIT(initial) { .count = (initial), .wq = WAIT_QUEUE_INIT }

void sem_give(struct semaphore *s);
void sem_take(struct semaphore *s);
int  sem_try_take(struct semaphore *s);

#endif
