/*
 * sched.h — Preemptive round-robin scheduler
 *
 * Tasks are preempted by SysTick → PendSV every tick.
 * Tasks can also yield voluntarily with sched_yield().
 */

#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define MAX_TASKS 6
#define TASK_STACK_SIZE 512

typedef void (*task_fn)(void);

/* Create a task. Returns task ID or -1 on failure. */
int sched_create_task(task_fn fn, const char *name);

/* Start the scheduler. Never returns. */
void sched_start(void);

/* Yield the CPU to the next ready task (cooperative). */
void sched_yield(void);

/* Called by PendSV: save old_sp, return new_sp (for preemption). */
uint32_t *sched_preempt(uint32_t *old_sp);

/* Get current task name. */
const char *sched_current_name(void);

/* Get current task ID. */
int sched_current_id(void);

/* Sleep for ms milliseconds (blocks current task). */
void sched_sleep_ms(uint32_t ms);

#endif
