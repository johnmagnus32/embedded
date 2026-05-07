/*
 * test_stress.c — Stress tests for scheduler and sync primitives
 */
#include "sched.h"
#include "sync.h"
#include "test.h"
#include <stdint.h>

extern uint32_t systick_get_ticks(void);

/* --- Test: rapid sleep/wake cycles --- */

static volatile int rapid_count;

static void rapid_sleeper(void)
{
    for (int i = 0; i < 200; i++) {
        sched_sleep_ms(1);
        rapid_count++;
    }
    while (1) sched_sleep_ms(60000);
}

/* --- Test: sem give/take from many tasks --- */

static struct semaphore stress_sem;
static volatile int sem_total;

static void sem_stress_worker(void)
{
    for (int i = 0; i < 50; i++) {
        sem_take(&stress_sem);
        sem_total++;
    }
    while (1) sched_sleep_ms(60000);
}

static void sem_stress_producer(void)
{
    for (int i = 0; i < 200; i++) {
        sem_give(&stress_sem);
        if (i % 10 == 0) sched_yield();
    }
    while (1) sched_sleep_ms(60000);
}

/* --- Test: yield storm — many tasks yielding rapidly --- */

static volatile int yield_storm_total;

static void yield_storm_task(void)
{
    for (int i = 0; i < 500; i++) {
        yield_storm_total++;
        sched_yield();
    }
    while (1) sched_sleep_ms(60000);
}

void test_stress_run(void)
{
    TEST_SUITE("stress");

    /* Test 1: rapid sleep/wake — 200 iterations of sleep(1) */
    rapid_count = 0;
    sched_create_task(rapid_sleeper, "rslp", 1);
    uint32_t t0 = systick_get_ticks();
    sched_sleep_ms(500);  /* should be enough for 200 x 1ms sleeps */
    uint32_t elapsed = systick_get_ticks() - t0;
    TEST_ASSERT_RANGE(rapid_count, 180, 210,
                      "200x sleep_ms(1) should complete ~200 iterations");
    TEST_ASSERT(elapsed >= 200, "200x sleep_ms(1) should take >= 200 ticks");

    /* Test 2: sem give/take from 4 consumers + 1 producer */
    stress_sem = (struct semaphore)SEM_INIT(0);
    sem_total = 0;
    sched_create_task(sem_stress_worker, "sw1", 1);
    sched_create_task(sem_stress_worker, "sw2", 1);
    sched_create_task(sem_stress_worker, "sw3", 1);
    sched_create_task(sem_stress_worker, "sw4", 1);
    sched_create_task(sem_stress_producer, "sprd", 1);
    sched_sleep_ms(1000);
    TEST_ASSERT_EQ(sem_total, 200,
                   "4 consumers x 50 takes = 200 from 200 gives");

    /* Test 3: yield storm — 4 tasks each yield 500 times */
    yield_storm_total = 0;
    sched_create_task(yield_storm_task, "ys1", 1);
    sched_create_task(yield_storm_task, "ys2", 1);
    sched_create_task(yield_storm_task, "ys3", 1);
    sched_create_task(yield_storm_task, "ys4", 1);
    sched_sleep_ms(500);
    TEST_ASSERT_EQ(yield_storm_total, 2000,
                   "4 tasks x 500 yields should all complete");

    uart_print("  stress tests done\n");
}
