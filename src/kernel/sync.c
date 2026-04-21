/*
 * sync.c — Mutex and semaphore implementation
 *
 * These are blocking primitives — if a task can't proceed, it yields
 * and retries on the next schedule. A real RTOS (Zephyr) would put
 * the task on a wait queue and only wake it when the resource is
 * available. We spin-yield for simplicity.
 *
 * We disable interrupts briefly (PRIMASK) to make check-and-modify
 * atomic. Same approach as Zephyr's k_spin_lock.
 */

#include "sync.h"
#include "sched.h"

/* Disable/enable interrupts for critical sections */
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

/* --- Mutex --- */

void mutex_lock(struct mutex *m)
{
    while (1) {
        uint32_t key = irq_lock();
        if (m->owner < 0) {
            /* Unlocked — claim it */
            extern int sched_current_id(void);
            m->owner = sched_current_id();
            irq_unlock(key);
            return;
        }
        irq_unlock(key);
        /* Owned by someone else — yield and retry */
        sched_yield();
    }
}

void mutex_unlock(struct mutex *m)
{
    uint32_t key = irq_lock();
    m->owner = -1;
    irq_unlock(key);
}

/* --- Semaphore --- */

void sem_give(struct semaphore *s)
{
    uint32_t key = irq_lock();
    s->count++;
    irq_unlock(key);
}

void sem_take(struct semaphore *s)
{
    while (1) {
        uint32_t key = irq_lock();
        if (s->count > 0) {
            s->count--;
            irq_unlock(key);
            return;
        }
        irq_unlock(key);
        sched_yield();
    }
}

int sem_try_take(struct semaphore *s)
{
    uint32_t key = irq_lock();
    if (s->count > 0) {
        s->count--;
        irq_unlock(key);
        return 0;
    }
    irq_unlock(key);
    return -1;
}
