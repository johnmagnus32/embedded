/*
 * test_sync.c — Semaphore and mutex tests
 */
#include "sched.h"
#include "sync.h"
#include "test.h"
#include <stdint.h>

/* --- Test: semaphore basic give/take --- */

static struct semaphore test_sem;
static volatile int sem_taken;

static void sem_taker_task(void)
{
    sem_take(&test_sem);  /* blocks until give */
    sem_taken = 1;
    while (1) sched_sleep_ms(10000);
}

/* --- Test: sem_try_take non-blocking --- */

/* --- Test: mutex protects critical section --- */

static struct mutex test_mutex;
static volatile int shared_counter;

static void mutex_inc_task(void)
{
    for (int i = 0; i < 100; i++) {
        mutex_lock(&test_mutex);
        int tmp = shared_counter;
        sched_yield();  /* yield while holding mutex — other task should block */
        shared_counter = tmp + 1;
        mutex_unlock(&test_mutex);
    }
    while (1) sched_sleep_ms(10000);
}

/* --- Test: semaphore as signaling mechanism --- */

static struct semaphore signal_sem;
static volatile int producer_done;
static volatile int consumer_got;

static void producer_task(void)
{
    sched_sleep_ms(50);
    producer_done = 1;
    sem_give(&signal_sem);
    while (1) sched_sleep_ms(10000);
}

static void consumer_task(void)
{
    sem_take(&signal_sem);  /* blocks until producer signals */
    consumer_got = producer_done;
    while (1) sched_sleep_ms(10000);
}

/* --- Test runner --- */

void test_sync_run(void)
{
    TEST_SUITE("sync");

    /* Test 1: sem_try_take on empty semaphore returns failure */
    test_sem = (struct semaphore)SEM_INIT(0);
    int ret = sem_try_take(&test_sem);
    TEST_ASSERT_EQ(ret, -1, "sem_try_take on empty sem should return -1");

    /* Test 2: sem_give then sem_try_take succeeds */
    sem_give(&test_sem);
    ret = sem_try_take(&test_sem);
    TEST_ASSERT_EQ(ret, 0, "sem_try_take after give should return 0");

    /* Test 3: sem_take blocks until sem_give */
    test_sem = (struct semaphore)SEM_INIT(0);
    sem_taken = 0;
    sched_create_task(sem_taker_task, "stk", 1);
    sched_sleep_ms(50);  /* let taker run and block */
    TEST_ASSERT_EQ(sem_taken, 0, "sem_take should block when count=0");
    sem_give(&test_sem);
    sched_sleep_ms(50);  /* let taker wake and run */
    TEST_ASSERT_EQ(sem_taken, 1, "sem_take should unblock after give");

    /* Test 4: mutex protects shared counter */
    test_mutex = (struct mutex)MUTEX_INIT;
    shared_counter = 0;
    sched_create_task(mutex_inc_task, "m1", 1);
    sched_create_task(mutex_inc_task, "m2", 1);
    sched_sleep_ms(500);  /* let both finish */
    TEST_ASSERT_EQ(shared_counter, 200, "mutex should protect counter (2 tasks x 100 increments)");

    /* Test 5: semaphore as producer-consumer signal */
    signal_sem = (struct semaphore)SEM_INIT(0);
    producer_done = 0;
    consumer_got = 0;
    sched_create_task(producer_task, "prod", 1);
    sched_create_task(consumer_task, "cons", 1);
    sched_sleep_ms(200);
    TEST_ASSERT_EQ(consumer_got, 1, "consumer should see producer's data after signal");

    uart_print("  sync tests done\n");
}
