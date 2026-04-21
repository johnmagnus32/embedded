/*
 * sync.h — Mutex and semaphore
 *
 * Mutex: mutual exclusion, only one task can hold it.
 *   Like Zephyr's k_mutex — tasks that try to lock a held mutex
 *   sleep until it's released.
 *
 * Semaphore: counting semaphore for signaling.
 *   Like Zephyr's k_sem — give() increments, take() decrements
 *   or sleeps if count is 0.
 */

#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

/* --- Mutex --- */

struct mutex {
    volatile int8_t owner;   /* task ID that holds it, -1 = unlocked */
    volatile uint8_t waiters; /* bitmask of tasks waiting */
};

#define MUTEX_INIT { .owner = -1, .waiters = 0 }

void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

/* --- Semaphore --- */

struct semaphore {
    volatile int16_t count;
    volatile uint8_t waiters;
};

#define SEM_INIT(initial) { .count = (initial), .waiters = 0 }

void sem_give(struct semaphore *s);
void sem_take(struct semaphore *s);

/* Try to take without blocking. Returns 0 on success, -1 if would block. */
int sem_try_take(struct semaphore *s);

#endif
