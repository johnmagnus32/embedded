/*
 * sched.c — Cooperative round-robin scheduler
 *
 * Context switch saves r4-r11 + sp into the task's TCB, then
 * restores the next task's registers. This is the same set of
 * registers that Zephyr's z_arm_pendsv saves/restores.
 *
 * On Cortex-M, r0-r3 and r12 are caller-saved (the C compiler
 * handles them), and lr/pc are managed by BL/BX instructions.
 * We only need to save the callee-saved set: r4-r11 + sp.
 */

#include "sched.h"
#include <stddef.h>

struct task_tcb {
    uint32_t *sp;           /* saved stack pointer */
    const char *name;
    uint8_t active;
};

static struct task_tcb tasks[MAX_TASKS];
static int num_tasks;
static int current_task = -1;

/* Stack memory for all tasks (in .bss — zeroed at boot) */
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(8)));

/*
 * Set up initial stack frame so the task "returns" into its entry function.
 * We fake a context that sched_switch_to will restore:
 *   - r4-r11 = 0 (don't care)
 *   - lr = task entry point (so the "return" from switch jumps to the task)
 *   - sp points to the saved registers
 */
static uint32_t *task_stack_init(uint8_t *stack_base, task_fn fn)
{
    /* Stack grows down — start at the top */
    uint32_t *sp = (uint32_t *)(stack_base + TASK_STACK_SIZE);

    /* Push fake context: r4, r5, r6, r7, r8, r9, r10, r11, lr */
    *(--sp) = (uint32_t)fn;   /* lr — "return address" = task entry */
    *(--sp) = 0;              /* r11 */
    *(--sp) = 0;              /* r10 */
    *(--sp) = 0;              /* r9 */
    *(--sp) = 0;              /* r8 */
    *(--sp) = 0;              /* r7 */
    *(--sp) = 0;              /* r6 */
    *(--sp) = 0;              /* r5 */
    *(--sp) = 0;              /* r4 */

    return sp;
}

int sched_create_task(task_fn fn, const char *name)
{
    if (num_tasks >= MAX_TASKS) return -1;

    int id = num_tasks++;
    tasks[id].sp = task_stack_init(task_stacks[id], fn);
    tasks[id].name = name;
    tasks[id].active = 1;
    return id;
}

const char *sched_current_name(void)
{
    if (current_task < 0) return "boot";
    return tasks[current_task].name;
}

/*
 * Assembly context switch.
 * Saves r4-r11 + lr to *from_sp, loads from *to_sp.
 *
 * void sched_context_switch(uint32_t **from_sp, uint32_t **to_sp);
 */
__attribute__((naked))
void sched_context_switch(uint32_t **from_sp, uint32_t **to_sp)
{
    __asm volatile(
        /* Save current context */
        "push {r4-r11, lr}      \n"
        "str  sp, [r0]          \n"  /* *from_sp = current sp */

        /* Load next context */
        "ldr  sp, [r1]          \n"  /* sp = *to_sp */
        "pop  {r4-r11, lr}      \n"
        "bx   lr                \n"  /* "return" into the new task */
    );
}

static void schedule_next(void)
{
    int prev = current_task;
    int next = (current_task + 1) % num_tasks;

    /* Round-robin: find next active task */
    for (int i = 0; i < num_tasks; i++) {
        int candidate = (current_task + 1 + i) % num_tasks;
        if (tasks[candidate].active) {
            next = candidate;
            break;
        }
    }

    if (next == prev) return;  /* only one task, no switch needed */

    current_task = next;
    sched_context_switch(&tasks[prev].sp, &tasks[next].sp);
}

void sched_yield(void)
{
    schedule_next();
}

/*
 * Start the scheduler by switching to the first task.
 * We fake a "previous task" context on the current (boot) stack.
 */
void sched_start(void)
{
    if (num_tasks == 0) return;

    /* Create a dummy TCB for the boot context (we'll never return to it) */
    static uint32_t *boot_sp;
    current_task = 0;
    sched_context_switch(&boot_sp, &tasks[0].sp);

    /* Never reached */
    while (1) {}
}
