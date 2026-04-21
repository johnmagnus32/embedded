/*
 * systick.c — SysTick + PendSV for Cortex-M4
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_CPU_CORTEX_M4

#include "sched.h"

#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)
#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)
#define SCB_SHPR3  (*(volatile uint32_t *)0xE000ED20)

#define ICSR_PENDSVSET  (1 << 28)

static volatile uint32_t tick_count;

uint32_t systick_get_ticks(void)
{
    return tick_count;
}

void systick_init(uint32_t cpu_hz, uint32_t tick_hz)
{
    SCB_SHPR3 |= (0xFFU << 16);
    SYST_RVR = (cpu_hz / tick_hz) - 1;
    SYST_CVR = 0;
    SYST_CSR = 0x07;
}

void systick_handler(void)
{
    tick_count++;
    SCB_ICSR = ICSR_PENDSVSET;
}

void pendsv_handler(void) __attribute__((naked));
void pendsv_handler(void)
{
    __asm volatile(
        "mrs   r0, psp             \n"
        "stmdb r0!, {r4-r11}       \n"
        "bl    sched_preempt       \n"
        "ldmia r0!, {r4-r11}       \n"
        "msr   psp, r0             \n"
        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
    );
}

#endif /* CONFIG_CPU_CORTEX_M4 */
