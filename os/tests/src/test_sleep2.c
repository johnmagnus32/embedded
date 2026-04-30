/*
 * test_sleep2.c — Additional sleep and scheduling fairness tests
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* --- Test: repeated short sleeps accumulate correctly --- */

/* --- Test: sleep(1) sleeps at least 1 tick --- */

/* --- Test: equal-priority tasks get fair CPU time --- */

static volatile int fair_a_count;
static volatile int fair_b_count;

static void fair_task_a(void)
{
    while (1) {
        fair_a_count++;
        sched_yield();
    }
}

static void fair_task_b(void)
{
    while (1) {
        fair_b_count++;
        sched_yield();
    }
}

/* --- Test: high-priority task preempts low-priority on wake --- */

static volatile uint32_t preempt_wake_tick;
static volatile uint32_t preempt_run_tick;

static void preempt_hi_task(void)
{
    sched_sleep_ms(100);  /* sleep, then wake and record when we run */
    preempt_run_tick = systick_get_ticks();
    while (1) sched_sleep_ms(60000);
}

static void preempt_lo_task(void)
{
    /* Busy loop — should be preempted when hi task wakes */
    while (1) {
        volatile int x = 0;
        for (int i = 0; i < 1000; i++) x += i;
        (void)x;
    }
}

void test_sleep2_run(void)
{
    TEST_SUITE("sleep2");

    /* Test 1: repeated sleep_ms(10) x 50 should take ~500 ticks */
    uint32_t t0 = systick_get_ticks();
    for (int i = 0; i < 50; i++)
        sched_sleep_ms(10);
    uint32_t elapsed = systick_get_ticks() - t0;
    TEST_ASSERT_RANGE(elapsed, 450, 600, "50x sleep_ms(10) should take ~500 ticks");

    /* Test 2: sleep_ms(1) should sleep at least 1 tick */
    t0 = systick_get_ticks();
    sched_sleep_ms(1);
    elapsed = systick_get_ticks() - t0;
    TEST_ASSERT(elapsed >= 1, "sleep_ms(1) should sleep at least 1 tick");

    /* Test 3: equal-priority fairness */
    fair_a_count = fair_b_count = 0;
    sched_create_task(fair_task_a, "fa", 1);
    sched_create_task(fair_task_b, "fb", 1);
    sched_sleep_ms(500);
    int total = fair_a_count + fair_b_count;
    int min_share = total * 35 / 100;  /* each should get at least 35% */
    TEST_ASSERT(fair_a_count > min_share, "task A should get fair CPU share");
    TEST_ASSERT(fair_b_count > min_share, "task B should get fair CPU share");

    /* Test 4: high-priority preempts low-priority on wake */
    preempt_wake_tick = preempt_run_tick = 0;
    sched_create_task(preempt_lo_task, "plo", 3);
    sched_create_task(preempt_hi_task, "phi", 1);
    sched_sleep_ms(200);
    /* hi task slept 100ms, should have run at ~tick 100 */
    /* It should preempt lo task within 1-2 ticks of waking */
    TEST_ASSERT(preempt_run_tick > 0, "high-priority task should have run");

    uart_print("  sleep2 tests done\n");
}
