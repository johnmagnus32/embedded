/*
 * test_sched.c — Scheduler tests
 *
 * Conservative with task slots — reuses tasks by letting them
 * sleep forever after their work is done.
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* Shared state for tests */
static volatile int run_order[4];
static volatile int run_idx;

static void prio_high_task(void)
{
    run_order[run_idx++] = 1;
    while (1) sched_sleep_ms(60000);
}

static void prio_low_task(void)
{
    run_order[run_idx++] = 2;
    while (1) sched_sleep_ms(60000);
}

void test_sched_run(void)
{
    TEST_SUITE("scheduler");

    /* Test 1: priority ordering — high prio task runs before low prio */
    run_idx = 0;
    run_order[0] = run_order[1] = 0;
    sched_create_task(prio_low_task, "lo", 3);
    sched_create_task(prio_high_task, "hi", 2);
    sched_sleep_ms(50);  /* yield — both should run, high first */
    TEST_ASSERT_EQ(run_order[0], 1, "high priority task should run first");
    TEST_ASSERT_EQ(run_order[1], 2, "low priority task should run second");

    /* Test 2: sched_current_name */
    const char *name = sched_current_name();
    TEST_ASSERT(name[0] == 't' && name[1] == 'e', "current task name should be 'test'");

    /* Test 3: systick is ticking */
    uint32_t t0 = systick_get_ticks();
    TEST_ASSERT(t0 > 0, "systick should be running");

    uart_print("  scheduler tests done\n");
}
