/*
 * test_sched.c — Scheduler tests
 *
 * Tests run as tasks within the scheduler. The test_runner task
 * creates worker tasks, waits for results, and checks assertions.
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* --- Test: tasks run in priority order --- */

static volatile int run_order[4];
static volatile int run_idx;

static void high_prio_task(void)
{
    run_order[run_idx++] = 0;  /* I'm task 0 (high priority) */
    while (1) sched_yield();
}

static void low_prio_task(void)
{
    run_order[run_idx++] = 1;  /* I'm task 1 (low priority) */
    while (1) sched_yield();
}

/* --- Test: yield gives CPU to equal-priority peer --- */

static volatile int yield_a_count;
static volatile int yield_b_count;

static void yield_task_a(void)
{
    for (int i = 0; i < 10; i++) {
        yield_a_count++;
        sched_yield();
    }
    while (1) sched_sleep_ms(10000);
}

static void yield_task_b(void)
{
    for (int i = 0; i < 10; i++) {
        yield_b_count++;
        sched_yield();
    }
    while (1) sched_sleep_ms(10000);
}

/* --- Test: sched_current_name returns correct name --- */

static volatile int name_ok;

static void named_task(void)
{
    const char *n = sched_current_name();
    /* Simple string compare */
    name_ok = (n[0] == 't' && n[1] == '_' && n[2] == 'n' && n[3] == 'a');
    while (1) sched_sleep_ms(10000);
}

/* --- Test runner (runs as highest priority task) --- */

void test_sched_run(void)
{
    TEST_SUITE("scheduler");

    /* Test 1: priority ordering
     * Create high (prio 1) and low (prio 2) tasks.
     * High should run first after we yield. */
    run_idx = 0;
    sched_create_task(high_prio_task, "hi", 1);
    sched_create_task(low_prio_task, "lo", 2);
    /* Yield to let them run — high prio runs first */
    sched_yield();
    sched_yield();  /* let low prio run too */
    TEST_ASSERT_EQ(run_order[0], 0, "high priority task should run first");
    TEST_ASSERT_EQ(run_order[1], 1, "low priority task should run second");

    /* Test 2: yield round-robin among equal priorities */
    yield_a_count = yield_b_count = 0;
    sched_create_task(yield_task_a, "ya", 1);
    sched_create_task(yield_task_b, "yb", 1);
    /* Let them run for a bit */
    sched_sleep_ms(100);
    TEST_ASSERT_EQ(yield_a_count, 10, "yield_task_a should run 10 times");
    TEST_ASSERT_EQ(yield_b_count, 10, "yield_task_b should run 10 times");

    /* Test 3: sched_current_name */
    name_ok = 0;
    sched_create_task(named_task, "t_na", 1);
    sched_sleep_ms(50);
    TEST_ASSERT(name_ok, "sched_current_name should return task name");

    uart_print("  scheduler tests done\n");
}
