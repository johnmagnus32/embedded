/*
 * systick_m0plus.c — SysTick + PendSV for Cortex-M0+
 *
 * M0+ differences from M4:
 *   - No stmdb/ldmia with r8-r11 directly
 *   - Must move r8-r11 to r0-r3 first, then push
 *   - No BASEPRI (only PRIMASK)
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_CPU_CORTEX_M0PLUS

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

/*
 * PendSV for Cortex-M0+
 * M0+ can't push/pop r8-r11 directly — must shuffle through r4-r7.
 */
void pendsv_handler(void) __attribute__((naked));
void pendsv_handler(void)
{
    __asm volatile(
        "mrs   r0, psp             \n"

        /* Save r4-r7 */
        "sub   r0, #16             \n"
        "stmia r0!, {r4-r7}        \n"

        /* Save r8-r11 via r4-r7 */
        "mov   r4, r8              \n"
        "mov   r5, r9              \n"
        "mov   r6, r10             \n"
        "mov   r7, r11             \n"
        "sub   r0, #32             \n"
        "stmia r0!, {r4-r7}        \n"

        "sub   r0, #16             \n"  /* r0 = start of saved area */

        "bl    sched_preempt       \n"  /* returns new sp in r0 */

        /* Restore r8-r11 */
        "ldmia r0!, {r4-r7}        \n"
        "mov   r8, r4              \n"
        "mov   r9, r5              \n"
        "mov   r10, r6             \n"
        "mov   r11, r7             \n"

        /* Restore r4-r7 */
        "ldmia r0!, {r4-r7}        \n"

        "msr   psp, r0             \n"

        "ldr   r0, =0xFFFFFFFD     \n"
        "bx    r0                  \n"
    );
}

#endif /* CONFIG_CPU_CORTEX_M0PLUS */
