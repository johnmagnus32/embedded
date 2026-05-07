/*
 * sync.c — Mutex and semaphore (SMP-safe)
 *
 * Uses spinlock instead of irq_lock for SMP safety.
 */

#include "sync.h"
#include "sched.h"
#include "spinlock.h"

static struct spinlock sync_lock = SPINLOCK_INIT(1);  /* use HW spinlock 1 */

void mutex_lock(struct mutex *m)
{
    while (1) {
        uint32_t key = spin_lock(&sync_lock);
        if (m->owner < 0) {
            m->owner = sched_current_id();
            spin_unlock(&sync_lock, key);
            return;
        }
        sched_block(&m->wq);
        spin_unlock(&sync_lock, key);
        sched_yield();
    }
}

void mutex_unlock(struct mutex *m)
{
    uint32_t key = spin_lock(&sync_lock);
    m->owner = -1;
    sched_wake(&m->wq);
    spin_unlock(&sync_lock, key);
}

void sem_give(struct semaphore *s)
{
    uint32_t key = spin_lock(&sync_lock);
    s->count++;
    sched_wake(&s->wq);
    spin_unlock(&sync_lock, key);
}

void sem_take(struct semaphore *s)
{
    while (1) {
        uint32_t key = spin_lock(&sync_lock);
        if (s->count > 0) {
            s->count--;
            spin_unlock(&sync_lock, key);
            return;
        }
        sched_block(&s->wq);
        spin_unlock(&sync_lock, key);
        sched_yield();
    }
}

int sem_try_take(struct semaphore *s)
{
    uint32_t key = spin_lock(&sync_lock);
    if (s->count > 0) {
        s->count--;
        spin_unlock(&sync_lock, key);
        return 0;
    }
    spin_unlock(&sync_lock, key);
    return -1;
}
