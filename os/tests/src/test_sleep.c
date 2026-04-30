/*
 * test_sleep.c — Sleep and timing tests
 */
#include "sched.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* --- Test: sleep accuracy (run from test_runner directly) --- */

/* --- Test: other tasks run during sleep --- */
static volatile int bg_counter;

static void background_counter(void)
{
    while (1) {
        bg_counter++;
        sched_yield();
    }
}

/* --- Test: wake ordering --- */
static volatile int wake_order[3];
static volatile int wake_idx;

static void sleep_short(void)
{
    sched_sleep_ms(50);
    wake_order[wake_idx++] = 1;
    while (1) sched_sleep_ms(60000);
}

static void sleep_medium(void)
{
    sched_sleep_ms(100);
    wake_order[wake_idx++] = 2;
    while (1) sched_sleep_ms(60000);
}

static void sleep_long(void)
{
    sched_sleep_ms(150);
    wake_order[wake_idx++] = 3;
    while (1) sched_sleep_ms(60000);
}

void test_sleep_run(void)
{
    TEST_SUITE("sleep");

    /* Test 1: sleep advances tick counter */
    uint32_t t0 = systick_get_ticks();
    sched_sleep_ms(200);
    uint32_t elapsed = systick_get_ticks() - t0;
    TEST_ASSERT_RANGE(elapsed, 180, 240, "sleep_ms(200) should take ~200 ticks");

    /* Test 2: longer sleep is proportionally longer */
    t0 = systick_get_ticks();
    sched_sleep_ms(500);
    elapsed = systick_get_ticks() - t0;
    TEST_ASSERT_RANGE(elapsed, 480, 540, "sleep_ms(500) should take ~500 ticks");

    /* Test 3: background task runs during sleep */
    bg_counter = 0;
    sched_create_task(background_counter, "bg", 2);
    sched_sleep_ms(100);
    TEST_ASSERT(bg_counter > 50, "background task should run during sleep");

    /* Test 4: wake order matches sleep duration */
    wake_idx = 0;
    wake_order[0] = wake_order[1] = wake_order[2] = 0;
    sched_create_task(sleep_long, "sl", 1);
    sched_create_task(sleep_short, "ss", 1);
    sched_create_task(sleep_medium, "sm", 1);
    sched_sleep_ms(250);
    TEST_ASSERT_EQ(wake_order[0], 1, "shortest sleeper should wake first");
    TEST_ASSERT_EQ(wake_order[1], 2, "medium sleeper should wake second");
    TEST_ASSERT_EQ(wake_order[2], 3, "longest sleeper should wake third");

    uart_print("  sleep tests done\n");
}
