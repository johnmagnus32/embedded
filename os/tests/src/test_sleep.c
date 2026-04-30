/*
 * test_sleep.c — Sleep and timing tests
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* --- Test: sleep_ms accuracy --- */

static volatile uint32_t sleep_start_tick;
static volatile uint32_t sleep_end_tick;

static void sleep_100ms_task(void)
{
    sleep_start_tick = systick_get_ticks();
    sched_sleep_ms(100);
    sleep_end_tick = systick_get_ticks();
    while (1) sched_sleep_ms(10000);
}

/* --- Test: sleep doesn't busy-wait (other tasks run during sleep) --- */

static volatile int idle_ran_during_sleep;

static void counter_task(void)
{
    while (1) {
        idle_ran_during_sleep++;
        sched_yield();
    }
}

static void sleeper_task(void)
{
    sched_sleep_ms(200);
    while (1) sched_sleep_ms(10000);
}

/* --- Test: multiple sleeps with different durations --- */

static volatile int wake_order[3];
static volatile int wake_idx;

static void sleep_50_task(void)
{
    sched_sleep_ms(50);
    wake_order[wake_idx++] = 50;
    while (1) sched_sleep_ms(10000);
}

static void sleep_100_task2(void)
{
    sched_sleep_ms(100);
    wake_order[wake_idx++] = 100;
    while (1) sched_sleep_ms(10000);
}

static void sleep_150_task(void)
{
    sched_sleep_ms(150);
    wake_order[wake_idx++] = 150;
    while (1) sched_sleep_ms(10000);
}

/* --- Test: systick advances correctly --- */

void test_sleep_run(void)
{
    TEST_SUITE("sleep");

    /* Test 1: systick is ticking */
    uint32_t t0 = systick_get_ticks();
    sched_sleep_ms(50);
    uint32_t t1 = systick_get_ticks();
    uint32_t elapsed = t1 - t0;
    TEST_ASSERT_RANGE(elapsed, 48, 55, "sleep_ms(50) should take ~50 ticks");

    /* Test 2: sleep_ms(100) accuracy */
    sleep_start_tick = sleep_end_tick = 0;
    sched_create_task(sleep_100ms_task, "s100", 1);
    sched_sleep_ms(200);  /* wait for it to finish */
    elapsed = sleep_end_tick - sleep_start_tick;
    TEST_ASSERT_RANGE(elapsed, 98, 105, "sleep_ms(100) should take ~100 ticks");

    /* Test 3: other tasks run during sleep */
    idle_ran_during_sleep = 0;
    sched_create_task(counter_task, "cnt", 2);
    sched_create_task(sleeper_task, "slp", 1);
    sched_sleep_ms(300);
    TEST_ASSERT(idle_ran_during_sleep > 100,
                "counter task should run many times during 200ms sleep");

    /* Test 4: wake order matches sleep duration */
    wake_idx = 0;
    sched_create_task(sleep_150_task, "s150", 1);
    sched_create_task(sleep_50_task, "s50", 1);
    sched_create_task(sleep_100_task2, "s1002", 1);
    sched_sleep_ms(250);
    TEST_ASSERT_EQ(wake_order[0], 50, "50ms sleeper should wake first");
    TEST_ASSERT_EQ(wake_order[1], 100, "100ms sleeper should wake second");
    TEST_ASSERT_EQ(wake_order[2], 150, "150ms sleeper should wake third");

    uart_print("  sleep tests done\n");
}
