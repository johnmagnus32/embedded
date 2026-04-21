/*
 * sync.c — Mutex and semaphore with proper blocking
 *
 * Before: sem_take → check → yield → check → yield → ... (wastes CPU)
 * After:  sem_take → block → (not scheduled) → sem_give wakes us → run
 *
 * This is how Zephyr's k_sem_take and Linux's down() work.
 */

#include "sync.h"
#include "sched.h"

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
            m->owner = sched_current_id();
            irq_unlock(key);
            return;
        }
        /* Block on the mutex's wait queue */
        sched_block(&m->wq);
        irq_unlock(key);
        sched_yield();  /* trigger PendSV — we won't be scheduled */
    }
}

void mutex_unlock(struct mutex *m)
{
    uint32_t key = irq_lock();
    m->owner = -1;
    sched_wake(&m->wq);  /* wake one waiter if any */
    irq_unlock(key);
}

/* --- Semaphore --- */

void sem_give(struct semaphore *s)
{
    uint32_t key = irq_lock();
    s->count++;
    sched_wake(&s->wq);  /* wake one blocked taker */
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
        /* Block until sem_give wakes us */
        sched_block(&s->wq);
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
