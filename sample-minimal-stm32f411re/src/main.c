/*
 * main.c — Interactive RTOS demo: type a/b/c to wake tasks
 *
 * Three tasks (task_a, task_b, task_c) sleep on semaphores.
 * The UART RX interrupt reads a character and gives the
 * corresponding semaphore, waking the task.
 *
 * In another terminal:  cat /tmp/sim_uart
 * Type a, b, or c and press Enter to wake a task.
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sched.h"
#include "sync.h"

DEVICE_DT_DECLARE(usart2);

static const struct device *uart;

/* Semaphores — one per task, given by the input handler */
static struct semaphore sem_a = SEM_INIT(0);
static struct semaphore sem_b = SEM_INIT(0);
static struct semaphore sem_c = SEM_INIT(0);

static void uart_print(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_poll_out(uart, '\r');
        uart_poll_out(uart, *s++);
    }
}

/* ---- Input handler (called from idle loop) ---- */

extern struct semaphore uart_rx_sem;  /* from UART driver */

static void process_input(void)
{
    char c;
    while (uart_poll_in(uart, &c) == 0) {
        switch (c) {
        case 'a': case 'A':
            uart_print(">> waking task_a\n");
            sem_give(&sem_a);
            break;
        case 'b': case 'B':
            uart_print(">> waking task_b\n");
            sem_give(&sem_b);
            break;
        case 'c': case 'C':
            uart_print(">> waking task_c\n");
            sem_give(&sem_c);
            break;
        default:
            break;
        }
    }
}

/* ---- Tasks ---- */

static void task_a(void)
{
    while (1) {
        sem_take(&sem_a);
        uart_print("task_a: woke up!\n");
    }
}

static void task_b(void)
{
    while (1) {
        sem_take(&sem_b);
        uart_print("task_b: woke up!\n");
    }
}

static void task_c(void)
{
    while (1) {
        sem_take(&sem_c);
        uart_print("task_c: woke up!\n");
    }
}

static void idle_task(void)
{
    while (1) {
        /* Check for UART input in idle — process_input wakes tasks */
        extern struct semaphore uart_rx_sem;
        sem_take(&uart_rx_sem);
        process_input();
    }
}

/* ---- Boot ---- */

void main(void)
{
    uart = DEVICE_DT_GET(usart2);

    uart_print("Interactive RTOS demo\n");
    uart_print("Type a, b, or c to wake a task\n\n");

    sched_create_task(task_a,    "task_a", 1);
    sched_create_task(task_b,    "task_b", 1);
    sched_create_task(task_c,    "task_c", 1);
    sched_create_task(idle_task, "idle",   255);

    extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
    systick_init(DT_SYSCLK_HZ, 1000);

    sched_start();
}
