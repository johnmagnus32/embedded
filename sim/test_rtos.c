/*
 * test_rtos.c — Minimal RTOS with 2 tasks for the emulator
 *
 * No OS library — everything inline so we can test the emulator's
 * interrupt handling (SysTick, PendSV, context switch).
 */

#include <stdint.h>

/* Peripheral registers */
#define USART2_SR   (*(volatile uint32_t *)0x40004400)
#define USART2_DR   (*(volatile uint32_t *)0x40004404)
#define USART2_BRR  (*(volatile uint32_t *)0x40004408)
#define USART2_CR1  (*(volatile uint32_t *)0x4000440C)

#define SYST_CSR    (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR    (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR    (*(volatile uint32_t *)0xE000E018)
#define SCB_ICSR    (*(volatile uint32_t *)0xE000ED04)
#define SCB_SHPR3   (*(volatile uint32_t *)0xE000ED20)

#define PENDSVSET   (1 << 28)

/* UART output */
static void uart_putc(char c)
{
    while (!(USART2_SR & (1 << 7))) ;
    USART2_DR = c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

/* ---- Minimal scheduler ---- */

#define MAX_TASKS 3
#define STACK_SIZE 256

typedef void (*task_fn)(void);

struct tcb {
    uint32_t *sp;
    const char *name;
};

static struct tcb tasks[MAX_TASKS];
static int current_task = 0;
static int num_tasks = 0;
static uint8_t stacks[MAX_TASKS][STACK_SIZE] __attribute__((aligned(8)));

static volatile uint32_t tick_count = 0;

static uint32_t *init_stack(uint8_t *base, task_fn fn)
{
    uint32_t *sp = (uint32_t *)(base + STACK_SIZE);
    /* Exception frame (popped by hardware) */
    *(--sp) = (1 << 24);       /* xPSR — Thumb bit */
    *(--sp) = (uint32_t)fn;    /* PC */
    *(--sp) = 0;               /* LR */
    *(--sp) = 0;               /* R12 */
    *(--sp) = 0;               /* R3 */
    *(--sp) = 0;               /* R2 */
    *(--sp) = 0;               /* R1 */
    *(--sp) = 0;               /* R0 */
    /* Software-saved (popped by PendSV) */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; /* R11-R8 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; /* R7-R4 */
    return sp;
}

static void create_task(const char *name, task_fn fn)
{
    int id = num_tasks++;
    tasks[id].sp = init_stack(stacks[id], fn);
    tasks[id].name = name;
}

/* Called by PendSV — save old SP, pick next, return new SP */
uint32_t *sched_switch(uint32_t *old_sp)
{
    tasks[current_task].sp = old_sp;
    current_task = (current_task + 1) % num_tasks;
    return tasks[current_task].sp;
}

/* ---- Interrupt handlers ---- */

void systick_handler(void)
{
    tick_count++;
    SCB_ICSR = PENDSVSET;  /* pend PendSV for context switch */
}

/* PendSV — context switch (naked function) */
void pendsv_handler(void) __attribute__((naked));
void pendsv_handler(void)
{
    __asm volatile(
        "mrs   r0, psp             \n"
        "stmdb r0!, {r4-r11}       \n"
        "bl    sched_switch         \n"
        "ldmia r0!, {r4-r11}       \n"
        "msr   psp, r0             \n"
        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
    );
}

/* ---- Busy delay using tick_count ---- */

static void delay_ticks(uint32_t ticks)
{
    uint32_t start = tick_count;
    while ((tick_count - start) < ticks) ;
}

/* ---- Tasks ---- */

static void task_a(void)
{
    /* Start SysTick from within the first task — safe because
     * we're now running on PSP with a proper stack frame */
    SYST_RVR = 500;
    SYST_CVR = 0;
    SYST_CSR = 0x07;

    while (1) {
        uart_puts("A\n");
        /* Just spin — SysTick will preempt us */
        for (volatile int i = 0; i < 100; i++) ;
    }
}

static void task_b(void)
{
    while (1) {
        uart_puts("B\n");
        for (volatile int i = 0; i < 100; i++) ;
    }
}

static void idle_task(void)
{
    while (1) ;
}

/* ---- Boot ---- */

void main(void)
{
    uart_puts("RTOS starting...\n");

    create_task("sensor", task_a);
    create_task("comms",  task_b);
    create_task("idle",   idle_task);

    /* Set PendSV to lowest priority */
    SCB_SHPR3 |= (0xFF << 16);

    uart_puts("Starting scheduler...\n");

    /* Start first task — SysTick will be started by task_a */
    uint32_t *sp = tasks[0].sp;
    __asm volatile(
        "cpsid i                   \n"  /* disable interrupts */
        "ldmia %0!, {r4-r11}       \n"
        "msr   psp, %0             \n"
        "movs  r0, #2              \n"
        "msr   control, r0         \n"
        "isb                       \n"
        "ldr   r0, =0xFFFFFFFD     \n"
        "cpsie i                   \n"  /* re-enable right before exc_return */
        "bx    r0                  \n"
        :
        : "r" (sp)
    );

    while (1) ;
}

/* Vector table */
extern unsigned int _stack_top;

__attribute__((section(".vector_table"), used))
const void *vectors[] = {
    (void *)0x20020000,     /* SP */
    (void *)main,           /* Reset */
    0, 0, 0, 0, 0,         /* NMI, HardFault, MemManage, BusFault, UsageFault */
    0, 0, 0, 0,            /* Reserved */
    0,                      /* SVCall */
    0, 0,                   /* Reserved */
    (void *)pendsv_handler, /* PendSV (vector 14) */
    (void *)systick_handler,/* SysTick (vector 15) */
};
