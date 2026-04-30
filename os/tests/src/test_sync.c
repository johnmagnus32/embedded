/*
 * test_sync.c — Semaphore and mutex tests
 */
#include "sched.h"
#include "sync.h"
#include "test.h"
#include <stdint.h>

/* --- Semaphore tests --- */

static struct semaphore test_sem;
static volatile int sem_taken;

static void sem_taker(void)
{
    sem_take(&test_sem);  /* blocks until give */
    sem_taken = 1;
    while (1) sched_sleep_ms(60000);
}

/* --- Mutex tests --- */

static struct mutex test_mutex;
static volatile int shared_counter;

static void mutex_worker(void)
{
    for (int i = 0; i < 100; i++) {
        mutex_lock(&test_mutex);
        int tmp = shared_counter;
        sched_yield();  /* yield while holding — other task should block */
        shared_counter = tmp + 1;
        mutex_unlock(&test_mutex);
    }
    while (1) sched_sleep_ms(60000);
}

/* --- Producer/consumer --- */

static struct semaphore signal_sem;
static volatile int produced;
static volatile int consumed;

static void producer(void)
{
    sched_sleep_ms(50);
    produced = 1;
    sem_give(&signal_sem);
    while (1) sched_sleep_ms(60000);
}

static void consumer(void)
{
    sem_take(&signal_sem);
    consumed = produced;
    while (1) sched_sleep_ms(60000);
}

void test_sync_run(void)
{
    TEST_SUITE("sync");

    /* Test 1: sem_try_take on empty sem fails */
    test_sem = (struct semaphore)SEM_INIT(0);
    int ret = sem_try_take(&test_sem);
    TEST_ASSERT_EQ(ret, -1, "sem_try_take on empty should fail");

    /* Test 2: sem_give then try_take succeeds */
    sem_give(&test_sem);
    ret = sem_try_take(&test_sem);
    TEST_ASSERT_EQ(ret, 0, "sem_try_take after give should succeed");

    /* Test 3: sem_take blocks, sem_give unblocks */
    test_sem = (struct semaphore)SEM_INIT(0);
    sem_taken = 0;
    int tid = sched_create_task(sem_taker, "stk", 1);
    if (tid < 0) { uart_print("  debug: create failed!\n"); }
    sched_sleep_ms(50);
    TEST_ASSERT_EQ(sem_taken, 0, "sem_take should block when empty");
    sem_give(&test_sem);
    sched_sleep_ms(100);  /* longer wait */
    if (!sem_taken) {
        uart_print("  debug: sem.count="); print_int(test_sem.count);
        uart_print(" waiters="); print_int(test_sem.wq.waiters);
        /* Check task state */
        struct task_stats st;
        sched_get_task_stats(tid, &st);
        uart_print(" task_state="); print_int(st.state);
        uart_print("\n");
    }
    TEST_ASSERT_EQ(sem_taken, 1, "sem_take should unblock after give");

    /* Test 4: mutex protects shared counter */
    test_mutex = (struct mutex)MUTEX_INIT;
    shared_counter = 0;
    sched_create_task(mutex_worker, "mw1", 1);
    sched_create_task(mutex_worker, "mw2", 1);
    sched_sleep_ms(500);
    TEST_ASSERT_EQ(shared_counter, 200, "mutex should protect counter");

    /* Test 5: producer-consumer signaling */
    signal_sem = (struct semaphore)SEM_INIT(0);
    produced = consumed = 0;
    sched_create_task(producer, "prod", 1);
    sched_create_task(consumer, "cons", 1);
    sched_sleep_ms(200);
    TEST_ASSERT_EQ(consumed, 1, "consumer should get producer's signal");

    uart_print("  sync tests done\n");
}
