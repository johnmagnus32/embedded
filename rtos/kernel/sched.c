/*
 * sched.c — Preemptive scheduler with SMP support
 *
 * SMP changes from single-core:
 *   1. current_task is per-CPU (each core runs a different task)
 *   2. Ready queue protected by spinlock (not just irq_lock)
 *   3. pick_next() must skip tasks running on OTHER cores
 *   4. get_cpu_id() determines which core we're on
 *
 * On single-core (CONFIG_SMP=n), spinlock degrades to irq_lock
 * and there's only one current_task — no overhead.
 */

#include "sched.h"
#include "config.h"
#include "spinlock.h"
#include "trace.h"
#include <stddef.h>

extern uint32_t systick_get_ticks(void);

#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET  (1 << 28)

#ifndef CONFIG_SMP_NUM_CPUS
#define CONFIG_SMP_NUM_CPUS 1
#endif

struct task_tcb {
    uint32_t *sp;
    const char *name;
    enum task_state state;
    uint32_t wake_tick;
    int running_on_cpu;
    uint8_t priority;
    task_fn fn;                /* entry function (looked up by the entry wrapper) */
    /* Runtime stats */
    uint32_t total_ticks;
    uint32_t last_switch_in;
};

static struct task_tcb tasks[MAX_TASKS];
static int num_tasks;

/* Per-CPU state */
static int current_task_per_cpu[CONFIG_SMP_NUM_CPUS];

/* Spinlock protecting the ready queue (SMP-safe) */
static struct spinlock sched_lock = SPINLOCK_INIT(0);

static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(8)));

/*
 * Mark the current task dead and never return. Called (via task_entry) when a
 * task function returns — the equivalent of Zephyr's k_thread_abort() on a
 * thread that falls off z_thread_entry(). A dead task is skipped by pick_next,
 * so the scheduler keeps running the others.
 *
 * Previously the initial LR was 0, so a returning task branched to PC=0 and
 * took an INVSTATE HardFault (this bit the gameboy audio task when audio_stop
 * unblocked its get_buffer with NULL and it fell out of its loop).
 */
void sched_task_exit(void)
{
    uint32_t key = spin_lock(&sched_lock);
    int cpu = get_cpu_id();
    tasks[current_task_per_cpu[cpu]].state = TASK_DEAD;
    tasks[current_task_per_cpu[cpu]].running_on_cpu = -1;
    spin_unlock(&sched_lock, key);
    sched_yield();          /* switch away; pick_next skips TASK_DEAD */
    for (;;) sched_yield(); /* never resumed, but never fall through either */
}

/*
 * Entry wrapper — every task's initial PC. Runs the real entry function; if it
 * returns, terminates cleanly. Analogous to Zephyr's z_thread_entry(). The task
 * takes no arguments, so we look its fn up in the TCB rather than passing it in
 * registers (which the sched_start launch path would discard).
 */
static void task_entry(void)
{
    int id = current_task_per_cpu[get_cpu_id()];
    tasks[id].fn();
    sched_task_exit();
}

static uint32_t *task_stack_init(uint8_t *stack_base)
{
    uint32_t *sp = (uint32_t *)(stack_base + TASK_STACK_SIZE);

    *(--sp) = (1 << 24);                    /* xPSR (Thumb bit) */
    *(--sp) = (uint32_t)task_entry;         /* PC = entry wrapper */
    *(--sp) = (uint32_t)sched_task_exit | 1;/* LR = safe trap if wrapper ever returns (Thumb bit) */
    *(--sp) = 0;               /* R12 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;  /* R3-R0 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;  /* R11-R8 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;  /* R7-R4 */

    return sp;
}

int sched_create_task(task_fn fn, const char *name, uint8_t priority)
{
    uint32_t key = spin_lock(&sched_lock);
    if (num_tasks >= MAX_TASKS) {
        spin_unlock(&sched_lock, key);
        return -1;
    }
    int id = num_tasks++;
    tasks[id].fn = fn;
    tasks[id].sp = task_stack_init(task_stacks[id]);
    tasks[id].name = name;
    tasks[id].state = TASK_READY;
    tasks[id].wake_tick = 0;
    tasks[id].running_on_cpu = -1;
    tasks[id].priority = priority;
    spin_unlock(&sched_lock, key);
    return id;
}

const char *sched_current_name(void)
{
    return tasks[current_task_per_cpu[get_cpu_id()]].name;
}

int sched_current_id(void)
{
    return current_task_per_cpu[get_cpu_id()];
}

int sched_get_task_count(void)
{
    return num_tasks;
}

int sched_get_task_stats(int id, struct task_stats *stats)
{
    if (id < 0 || id >= num_tasks) return -1;
    stats->name = tasks[id].name;
    stats->priority = tasks[id].priority;
    stats->state = (uint8_t)tasks[id].state;
    stats->total_ticks = tasks[id].total_ticks;
    return 0;
}

/*
 * Pick the highest-priority READY task for this CPU.
 * Must be called with sched_lock held.
 *
 * Before (round-robin):
 *   Scan from current+1, pick first READY task.
 *   All tasks get equal time regardless of importance.
 *
 * After (priority):
 *   Scan ALL tasks, pick the one with lowest priority number.
 *   A priority-0 motor task always preempts a priority-5 shell task.
 *   Among equal priorities, round-robin by starting from current+1.
 *
 * This is how Zephyr works — highest priority ready thread always runs.
 */
static int pick_next(int cpu)
{
    uint32_t now = systick_get_ticks();
    int best = -1;
    uint8_t best_prio = 255;

    for (int i = 0; i < num_tasks; i++) {
        /* Start scanning from current+1 for round-robin among equal priorities */
        int c = (current_task_per_cpu[cpu] + 1 + i) % num_tasks;
        struct task_tcb *t = &tasks[c];

        /* Wake sleeping tasks */
        if (t->state == TASK_SLEEPING && now >= t->wake_tick) {
            t->state = TASK_READY;
            t->wake_tick = 0;
        }

        /* Skip tasks running on another CPU */
        if (t->running_on_cpu >= 0 && t->running_on_cpu != cpu)
            continue;

        /* Pick highest priority (lowest number) ready task.
         * TASK_DEAD/BLOCKED/SLEEPING are never READY, so never picked here. */
        if (t->state == TASK_READY && t->priority <= best_prio) {
            best = c;
            best_prio = t->priority;
        }
    }

    if (best >= 0)
        return best;

    /* Nothing READY. Do NOT fall back to the current task if it is no longer
     * runnable (e.g. it just called sched_task_exit / blocked) — resuming a
     * dead/blocked context is what caused the PC=0 crash. Prefer staying on the
     * current task only if it is still runnable; otherwise this build has no
     * idle task and we must not schedule anything non-runnable. Callers should
     * always provide an always-READY idle task; if not, keep the current task
     * parked is impossible, so we return current and rely on it being the idle. */
    int cur = current_task_per_cpu[cpu];
    if (tasks[cur].state == TASK_RUNNING || tasks[cur].state == TASK_READY)
        return cur;

    /* Last resort: scan for ANY runnable task ignoring priority/round-robin. */
    for (int i = 0; i < num_tasks; i++)
        if (tasks[i].state == TASK_READY && tasks[i].running_on_cpu < 0)
            return i;

    /* Truly nothing runnable — return current (should be unreachable when an
     * idle task exists). The caller will resume it; a well-formed system always
     * has an idle task so we never get here. */
    return cur;
}

/*
 * Called by PendSV on each core.
 */
uint32_t *sched_preempt(uint32_t *old_sp)
{
    int cpu = get_cpu_id();
    int old_task = current_task_per_cpu[cpu];
    uint32_t now = systick_get_ticks();

    uint32_t key = spin_lock(&sched_lock);
    trace_begin("scheduler");

    /* Account CPU time to outgoing task */
    tasks[old_task].total_ticks += now - tasks[old_task].last_switch_in;

    tasks[old_task].sp = old_sp;
    if (tasks[old_task].state == TASK_RUNNING) {
        tasks[old_task].state = TASK_READY;
        tasks[old_task].running_on_cpu = -1;
    }

    int next = pick_next(cpu);
    current_task_per_cpu[cpu] = next;
    tasks[next].state = TASK_RUNNING;
    tasks[next].running_on_cpu = cpu;
    tasks[next].last_switch_in = now;

    trace_begin(tasks[next].name);

#ifdef CONFIG_MPU
    extern void mpu_switch_task(uint32_t stack_base, uint32_t stack_size);
    mpu_switch_task((uint32_t)task_stacks[next], TASK_STACK_SIZE);
#endif

    spin_unlock(&sched_lock, key);

    return tasks[next].sp;
}

void sched_yield(void)
{
    SCB_ICSR = ICSR_PENDSVSET;
}

void sched_sleep_ms(uint32_t ms)
{
    uint32_t key = spin_lock(&sched_lock);
    int cpu = get_cpu_id();
    int id = current_task_per_cpu[cpu];
    uint32_t now = systick_get_ticks();
    tasks[id].wake_tick = now + ms;
    tasks[id].state = TASK_SLEEPING;
    tasks[id].running_on_cpu = -1;
    spin_unlock(&sched_lock, key);
    sched_yield();
}

void sched_block(struct wait_queue *wq)
{
    /* Must be called with sched_lock held by caller */
    int cpu = get_cpu_id();
    int id = current_task_per_cpu[cpu];
    tasks[id].state = TASK_BLOCKED;
    tasks[id].running_on_cpu = -1;
    wq->waiters |= (1 << id);
}

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
        if (wq->waiters & (1 << i))
            tasks[i].state = TASK_READY;
    }
    wq->waiters = 0;
}

void sched_start(void)
{
    if (num_tasks == 0) return;

    int cpu = get_cpu_id();
    current_task_per_cpu[cpu] = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].running_on_cpu = cpu;
    trace_begin(tasks[0].name);
    uint32_t *sp = tasks[0].sp;

#ifdef CONFIG_CPU_CORTEX_M0PLUS
    __asm volatile(
        "cpsid i                   \n"  /* disable interrupts during switch */
        "ldmia %0!, {r4-r7}        \n"
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
        "cpsie i                   \n"  /* re-enable: exc_return is next */
        "bx    r0                  \n"
        :
        : "r" (sp)
    );
#else
    __asm volatile(
        "cpsid i                   \n"
        "ldmia %0!, {r4-r11}       \n"  /* pop saved R4-R11 */
        "msr   psp, %0             \n"  /* set PSP to exception frame */
        "movs  r0, #2              \n"  /* SPSEL=1 (PSP) */
        "msr   control, r0         \n"
        "isb                       \n"
        /* Pop the exception frame manually (we're in thread mode,
         * can't use EXC_RETURN). PSP points to {R0-R3, R12, LR, PC, xPSR} */
        "mrs   r0, psp             \n"
        "ldmia r0!, {r1-r4}        \n"  /* pop R0-R3 into r1-r4 (scratch) */
        "ldmia r0!, {r1}           \n"  /* pop R12 (discard) */
        "ldmia r0!, {r1}           \n"  /* pop LR (discard) */
        "ldmia r0!, {r1}           \n"  /* pop PC → r1 */
        "ldmia r0!, {r2}           \n"  /* pop xPSR (discard) */
        "msr   psp, r0             \n"  /* update PSP past the frame */
        "cpsie i                   \n"
        "bx    r1                  \n"  /* jump to task entry */
        :
        : "r" (sp)
    );
#endif
    while (1) {}
}
