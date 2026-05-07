/*
 * test_idle_prio.c — Regression test for pick_next priority 255 bug
 *
 * Bug: pick_next() used (priority < best_prio) with best_prio=255,
 * so idle tasks at priority 255 were never selected. When all other
 * tasks were sleeping, the scheduler fell through to the current
 * (sleeping) task instead of switching to idle, causing it to
 * busy-loop through sleep_ms repeatedly.
 *
 * Detection: a task that calls sleep_ms(200) should execute its
 * loop body exactly once after waking. If the bug is present, the
 * task is re-scheduled immediately despite being SLEEPING, so it
 * re-enters sleep_ms thousands of times before actually waking.
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

static volatile int sleep_calls;
static volatile int wake_count;

static void sleep_counter_task(void)
{
    while (1) {
        sleep_calls++;
        sched_sleep_ms(200);
        wake_count++;
    }
}

void test_idle_prio_run(void)
{
    TEST_SUITE("idle_prio");

    sleep_calls = 0;
    wake_count = 0;

    sched_create_task(sleep_counter_task, "slc", 1);

    /* Wait long enough for the worker to sleep and wake twice */
    sched_sleep_ms(500);

    /*
     * With the fix: sleep_calls should be ~2-3 (called sleep, woke,
     * called sleep again, woke again).
     *
     * With the bug: sleep_calls would be thousands because the task
     * is re-scheduled immediately each time it calls sleep_ms.
     */
    TEST_ASSERT(sleep_calls < 20,
                "sleep_ms should not busy-loop (priority 255 idle must be selectable)");
    TEST_ASSERT(wake_count >= 1,
                "task should wake at least once after sleeping");

    uart_print("  idle_prio tests done\n");
}
