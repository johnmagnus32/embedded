/*
 * tasks.c — Demo tasks (a, b, idle)
 */

#include "sched.h"
#include "app.h"

void task_a(void)
{
    int count = 0;
    while (1) {
        uart_print("a:");
        print_int(count++);
        uart_print("\n");
        sched_sleep_ms(1000);
    }
}

void task_b(void)
{
    int count = 0;
    while (1) {
        uart_print("b:");
        print_int(count++);
        uart_print("\n");
        sched_sleep_ms(1000);
    }
}

void idle_task(void)
{
    while (1) {}
}
