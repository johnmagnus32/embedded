/*
 * sched.c — Preemptive round-robin scheduler
 *
 * Two context switch paths:
 *   1. Preemptive: SysTick → PendSV → sched_preempt()
 *      PendSV saves r4-r11 to PSP, calls us, we return new PSP
 *   2. Cooperative: task calls sched_yield() → triggers PendSV
 *
 * Tasks run using PSP (process stack pointer).
 * ISRs use MSP (main stack pointer) — set up by the vector table.
 */

#include "sched.h"
#include <stddef.h>

/* Forward declare systick_get_ticks */
extern uint32_t systick_get_ticks(void);

/* SCB register for pending PendSV */
#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET  (1 << 28)

struct task_tcb {
    uint32_t *sp;
    const char *name;
    uint8_t active;
    uint32_t wake_tick;     /* 0 = ready, >0 = sleeping until this tick */
};

static struct task_tcb tasks[MAX_TASKS];
static int num_tasks;
static int current_task;

static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(8)));

/*
 * Initial stack frame for a new task.
 *
 * When PendSV restores this task for the first time, it will:
 *   1. ldmia r0!, {r4-r11}  — pop our fake r4-r11 (zeros)
 *   2. msr psp, r0          — set PSP to point at the exception frame
 *   3. bx 0xFFFFFFFD        — return from exception using PSP
 *
 * The CPU then pops the exception frame: r0-r3, r12, lr, pc, xPSR
 * and jumps to pc = task entry function.
 *
 * So we need to set up both:
 *   - r4-r11 (popped by our PendSV code)
 *   - exception frame (popped by hardware on exception return)
 */
static uint32_t *task_stack_init(uint8_t *stack_base, task_fn fn)
{
    uint32_t *sp = (uint32_t *)(stack_base + TASK_STACK_SIZE);

    /* Hardware exception frame (popped by CPU on exception return) */
    *(--sp) = (1 << 24);       /* xPSR — Thumb bit must be set */
    *(--sp) = (uint32_t)fn;    /* PC — task entry point */
    *(--sp) = 0;               /* LR */
    *(--sp) = 0;               /* R12 */
    *(--sp) = 0;               /* R3 */
    *(--sp) = 0;               /* R2 */
    *(--sp) = 0;               /* R1 */
    *(--sp) = 0;               /* R0 */

    /* Software-saved registers (popped by our PendSV handler) */
    *(--sp) = 0;               /* R11 */
    *(--sp) = 0;               /* R10 */
    *(--sp) = 0;               /* R9 */
    *(--sp) = 0;               /* R8 */
    *(--sp) = 0;               /* R7 */
    *(--sp) = 0;               /* R6 */
    *(--sp) = 0;               /* R5 */
    *(--sp) = 0;               /* R4 */

    return sp;
}

int sched_create_task(task_fn fn, const char *name)
{
    if (num_tasks >= MAX_TASKS) return -1;

    int id = num_tasks++;
    tasks[id].sp = task_stack_init(task_stacks[id], fn);
    tasks[id].name = name;
    tasks[id].active = 1;
    tasks[id].wake_tick = 0;
    return id;
}

const char *sched_current_name(void)
{
    return tasks[current_task].name;
}

/* Pick the next ready task (round-robin, skip sleeping tasks) */
static int pick_next(void)
{
    uint32_t now = systick_get_ticks();

    for (int i = 1; i <= num_tasks; i++) {
        int candidate = (current_task + i) % num_tasks;
        struct task_tcb *t = &tasks[candidate];

        if (!t->active) continue;

        /* Wake up if sleep expired */
        if (t->wake_tick && now >= t->wake_tick) {
            t->wake_tick = 0;
        }

        if (t->wake_tick == 0) {
            return candidate;
        }
    }
    return current_task;  /* no other task ready — stay on current */
}

/*
 * Called by PendSV handler (from ISR context).
 * Saves old_sp into current TCB, picks next task, returns its sp.
 */
uint32_t *sched_preempt(uint32_t *old_sp)
{
    tasks[current_task].sp = old_sp;
    current_task = pick_next();
    return tasks[current_task].sp;
}

/* Cooperative yield: just pend PendSV */
void sched_yield(void)
{
    SCB_ICSR = ICSR_PENDSVSET;
}

/* Sleep: mark task as sleeping, then yield */
void sched_sleep_ms(uint32_t ms)
{
    tasks[current_task].wake_tick = systick_get_ticks() + ms;
    sched_yield();
}

/*
 * Start the scheduler.
 * Switch from MSP (boot stack) to PSP (task stacks).
 * Load the first task's context and "return" into it.
 */
void sched_start(void)
{
    if (num_tasks == 0) return;

    current_task = 0;
    uint32_t *sp = tasks[0].sp;

    __asm volatile(
        /* Restore r4-r11 from first task's stack */
        "ldmia %0!, {r4-r11}       \n"

        /* Set PSP to the exception frame */
        "msr   psp, %0             \n"

        /* Switch to PSP for thread mode (set CONTROL.SPSEL = 1) */
        "movs  r0, #2              \n"
        "msr   control, r0         \n"
        "isb                       \n"

        /* Return to thread mode using PSP — CPU pops exception frame */
        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
        :
        : "r" (sp)
    );

    /* Never reached */
    while (1) {}
}
