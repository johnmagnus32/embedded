/*
 * sched.h — Preemptive scheduler with wait queues
 *
 * Tasks can be in one of three states:
 *   READY    — in the ready queue, will be scheduled
 *   RUNNING  — currently executing on the CPU
 *   BLOCKED  — on a wait queue, will NOT be scheduled until woken
 *   SLEEPING — waiting for a timeout
 *
 * This is how Zephyr/Linux work — blocked tasks don't waste CPU.
 */

#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define MAX_TASKS 8
#define TASK_STACK_SIZE 512

typedef void (*task_fn)(void);

/* Task states */
enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
};

/*
 * Wait queue — a list of tasks waiting for something.
 * Like Zephyr's _wait_q_t / Linux's wait_queue_head_t.
 */
struct wait_queue {
    uint8_t waiters;  /* bitmask of task IDs waiting */
};

#define WAIT_QUEUE_INIT { .waiters = 0 }

int  sched_create_task(task_fn fn, const char *name);
void sched_start(void);
void sched_yield(void);
void sched_sleep_ms(uint32_t ms);

/* Block current task on a wait queue (called with IRQs locked) */
void sched_block(struct wait_queue *wq);

/* Wake one task from a wait queue (callable from ISR) */
void sched_wake(struct wait_queue *wq);

/* Wake all tasks from a wait queue */
void sched_wake_all(struct wait_queue *wq);

/* Called by PendSV */
uint32_t *sched_preempt(uint32_t *old_sp);

const char *sched_current_name(void);
int sched_current_id(void);

#endif
