/*
 * test_sync2.c — Additional sync tests
 */
#include "sched.h"
#include "sync.h"
#include "test.h"
#include <stdint.h>

/* --- Test: counting semaphore --- */

/* --- Test: multiple waiters on one semaphore --- */

static struct semaphore multi_sem;
static volatile int waiter_woke[3];

static void multi_waiter_0(void)
{
    sem_take(&multi_sem);
    waiter_woke[0] = 1;
    while (1) sched_sleep_ms(60000);
}

static void multi_waiter_1(void)
{
    sem_take(&multi_sem);
    waiter_woke[1] = 1;
    while (1) sched_sleep_ms(60000);
}

static void multi_waiter_2(void)
{
    sem_take(&multi_sem);
    waiter_woke[2] = 1;
    while (1) sched_sleep_ms(60000);
}

void test_sync2_run(void)
{
    TEST_SUITE("sync2");

    /* Test 1: counting semaphore — give 5, take 5 without blocking */
    struct semaphore csem = (struct semaphore)SEM_INIT(0);
    for (int i = 0; i < 5; i++)
        sem_give(&csem);
    int all_ok = 1;
    for (int i = 0; i < 5; i++) {
        if (sem_try_take(&csem) != 0) { all_ok = 0; break; }
    }
    TEST_ASSERT(all_ok, "5x give then 5x try_take should all succeed");
    int extra = sem_try_take(&csem);
    TEST_ASSERT_EQ(extra, -1, "6th try_take should fail (count exhausted)");

    /* Test 2: multiple waiters — 3 tasks blocked, give 3 times */
    multi_sem = (struct semaphore)SEM_INIT(0);
    waiter_woke[0] = waiter_woke[1] = waiter_woke[2] = 0;
    sched_create_task(multi_waiter_0, "mw0", 1);
    sched_create_task(multi_waiter_1, "mw1", 1);
    sched_create_task(multi_waiter_2, "mw2", 1);
    sched_sleep_ms(50);  /* let all 3 block */
    TEST_ASSERT_EQ(waiter_woke[0] + waiter_woke[1] + waiter_woke[2], 0,
                   "all 3 waiters should be blocked");
    sem_give(&multi_sem);
    sched_sleep_ms(50);
    TEST_ASSERT_EQ(waiter_woke[0] + waiter_woke[1] + waiter_woke[2], 1,
                   "1 give should wake exactly 1 waiter");
    sem_give(&multi_sem);
    sem_give(&multi_sem);
    sched_sleep_ms(50);
    TEST_ASSERT_EQ(waiter_woke[0] + waiter_woke[1] + waiter_woke[2], 3,
                   "3 gives should wake all 3 waiters");

    uart_print("  sync2 tests done\n");
}
