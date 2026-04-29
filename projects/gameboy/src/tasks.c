/*
 * tasks.c — Demo tasks (a, b, idle)
 */

#include "sched.h"
#include "log.h"

void task_a(void)
{
    int count = 0;
    while (1) {
        log_printf("a:%d", count++);
        sched_sleep_ms(1000);
    }
}

void task_b(void)
{
    int count = 0;
    while (1) {
        log_printf("b:%d", count++);
        sched_sleep_ms(1000);
    }
}

void idle_task(void)
{
    while (1) {}
}
