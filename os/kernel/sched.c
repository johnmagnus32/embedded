/*
 * sched.c — Preemptive scheduler with wait queues
 *
 * Key change from before: tasks can be BLOCKED on a wait queue.
 * Blocked tasks are skipped by pick_next() — they consume zero CPU.
 * sched_wake() moves a task from BLOCKED → READY.
 *
 * Before: sem_take → spin: check → yield → check → yield → ...
 * After:  sem_take → block → (not scheduled at all) → wake → run
 */

#include "sched.h"
#include "config.h"
#include <stddef.h>

extern uint32_t systick_get_ticks(void);

#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET  (1 << 28)

struct task_tcb {
    uint32_t *sp;
    const char *name;
    enum task_state state;
    uint32_t wake_tick;
};

static struct task_tcb tasks[MAX_TASKS];
static int num_tasks;
static int current_task;

static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(8)));

static uint32_t *task_stack_init(uint8_t *stack_base, task_fn fn)
{
    uint32_t *sp = (uint32_t *)(stack_base + TASK_STACK_SIZE);

    /* Hardware exception frame */
    *(--sp) = (1 << 24);       /* xPSR — Thumb bit */
    *(--sp) = (uint32_t)fn;    /* PC */
    *(--sp) = 0;               /* LR */
    *(--sp) = 0;               /* R12 */
    *(--sp) = 0;               /* R3 */
    *(--sp) = 0;               /* R2 */
    *(--sp) = 0;               /* R1 */
    *(--sp) = 0;               /* R0 */

    /* Software-saved registers */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;  /* R11-R8 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;  /* R7-R4 */

    return sp;
}

int sched_create_task(task_fn fn, const char *name)
{
    if (num_tasks >= MAX_TASKS) return -1;
    int id = num_tasks++;
    tasks[id].sp = task_stack_init(task_stacks[id], fn);
    tasks[id].name = name;
    tasks[id].state = TASK_READY;
    tasks[id].wake_tick = 0;
    return id;
}

const char *sched_current_name(void)
{
    return tasks[current_task].name;
}

int sched_current_id(void)
{
    return current_task;
}

/* Pick next READY task (skip BLOCKED and SLEEPING) */
static int pick_next(void)
{
    uint32_t now = systick_get_ticks();

    for (int i = 1; i <= num_tasks; i++) {
        int c = (current_task + i) % num_tasks;
        struct task_tcb *t = &tasks[c];

        /* Wake sleeping tasks whose timeout expired */
        if (t->state == TASK_SLEEPING && now >= t->wake_tick) {
            t->state = TASK_READY;
            t->wake_tick = 0;
        }

        if (t->state == TASK_READY)
            return c;
    }
    return current_task;  /* nothing else ready */
}

uint32_t *sched_preempt(uint32_t *old_sp)
{
    tasks[current_task].sp = old_sp;
    if (tasks[current_task].state == TASK_RUNNING)
        tasks[current_task].state = TASK_READY;

    current_task = pick_next();
    tasks[current_task].state = TASK_RUNNING;
    return tasks[current_task].sp;
}

void sched_yield(void)
{
    SCB_ICSR = ICSR_PENDSVSET;
}

void sched_sleep_ms(uint32_t ms)
{
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));

    tasks[current_task].wake_tick = systick_get_ticks() + ms;
    tasks[current_task].state = TASK_SLEEPING;

    __asm volatile("msr primask, %0" :: "r"(key));
    sched_yield();
}

/*
 * Block current task on a wait queue.
 * The task will NOT be scheduled until sched_wake() is called.
 * Must be called with interrupts locked by the caller.
 */
void sched_block(struct wait_queue *wq)
{
    int id = current_task;
    tasks[id].state = TASK_BLOCKED;
    wq->waiters |= (1 << id);
    /* Caller must call sched_yield() after unlocking IRQs */
}

/*
 * Wake one task from a wait queue (the first one found).
 * Safe to call from ISR context.
 */
void sched_wake(struct wait_queue *wq)
{
    if (wq->waiters == 0) return;

    for (int i = 0; i < num_tasks; i++) {
        if (wq->waiters & (1 << i)) {
            wq->waiters &= ~(1 << i);
            tasks[i].state = TASK_READY;
            return;
        }
    }
}

void sched_wake_all(struct wait_queue *wq)
{
    for (int i = 0; i < num_tasks; i++) {
        if (wq->waiters & (1 << i)) {
            tasks[i].state = TASK_READY;
        }
    }
    wq->waiters = 0;
}

void sched_start(void)
{
    if (num_tasks == 0) return;

    current_task = 0;
    tasks[0].state = TASK_RUNNING;
    uint32_t *sp = tasks[0].sp;

#ifdef CONFIG_CPU_CORTEX_M0PLUS
    /* M0+: can't ldmia r8-r11 directly */
    __asm volatile(
        /* Load r4-r7 */
        "ldmia %0!, {r4-r7}        \n"
        /* Load r8-r11 via r0-r3 (clobber is ok, we're switching) */
        "ldmia %0!, {r0-r3}        \n"
        "mov   r8, r0              \n"
        "mov   r9, r1              \n"
        "mov   r10, r2             \n"
        "mov   r11, r3             \n"
        "msr   psp, %0             \n"
        "movs  r0, #2              \n"
        "msr   control, r0         \n"
        "isb                       \n"
        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
        :
        : "r" (sp)
    );
#else
    __asm volatile(
        "ldmia %0!, {r4-r11}       \n"
        "msr   psp, %0             \n"
        "movs  r0, #2              \n"
        "msr   control, r0         \n"
        "isb                       \n"
        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
        :
        : "r" (sp)
    );
#endif
    while (1) {}
}
