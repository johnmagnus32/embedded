/*
 * sched.h — Simple cooperative scheduler
 *
 * Tasks run until they call sched_yield(). The scheduler picks the
 * next ready task in round-robin order. No preemption — a task that
 * never yields starves everything else.
 *
 * This is the simplest possible scheduler. Zephyr adds:
 *  - Priority-based preemption (SysTick interrupt forces a switch)
 *  - Sleep/timeout (k_msleep puts task on a wait queue)
 *  - Synchronization (semaphores, mutexes block a task)
 *
 * We do context switches by saving/restoring r4-r11 + sp (the
 * callee-saved registers on ARM). r0-r3 and lr are caller-saved
 * so the C calling convention handles them.
 */

#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define MAX_TASKS 4
#define TASK_STACK_SIZE 512

typedef void (*task_fn)(void);

/* Create a task. Returns task ID or -1 on failure. */
int sched_create_task(task_fn fn, const char *name);

/* Start the scheduler. Never returns. */
void sched_start(void);

/* Yield the CPU to the next ready task. */
void sched_yield(void);

/* Get current task name (for logging). */
const char *sched_current_name(void);

#endif
