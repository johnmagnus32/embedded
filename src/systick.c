/*
 * systick.c — SysTick timer + PendSV context switch
 *
 * SysTick fires every 1ms and pends PendSV.
 * PendSV is the lowest priority exception — it fires after all
 * other ISRs complete, ensuring a clean context switch point.
 *
 * This is exactly how Zephyr does it:
 *   SysTick ISR → sys_clock_announce() → z_arm_int_exit() → pend PendSV
 *   PendSV → z_arm_pendsv() → save/restore r4-r11, switch sp
 */

#include <stdint.h>
#include "sched.h"

/* Cortex-M system registers */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)  /* Control and Status */
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)  /* Reload Value */
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)  /* Current Value */
#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)  /* Interrupt Control State */
#define SCB_SHPR3  (*(volatile uint32_t *)0xE000ED20)  /* System Handler Priority 3 */

#define ICSR_PENDSVSET  (1 << 28)

/* Tick counter */
static volatile uint32_t tick_count;

uint32_t systick_get_ticks(void)
{
    return tick_count;
}

void systick_init(uint32_t cpu_hz, uint32_t tick_hz)
{
    /* Set PendSV to lowest priority (0xFF) so it only fires
     * when no other ISR is active — safe context switch point */
    SCB_SHPR3 |= (0xFFU << 16);  /* PendSV priority = bits 23:16 */

    /* Configure SysTick: reload = cpu_hz / tick_hz - 1 */
    SYST_RVR = (cpu_hz / tick_hz) - 1;
    SYST_CVR = 0;
    /* Enable: bit 0 = enable, bit 1 = interrupt, bit 2 = use CPU clock */
    SYST_CSR = 0x07;
}

/*
 * SysTick ISR — fires every tick (1ms).
 * Just increments the counter and pends PendSV for context switch.
 */
void systick_handler(void)
{
    tick_count++;
    /* Pend PendSV — the actual context switch happens there */
    SCB_ICSR = ICSR_PENDSVSET;
}

/*
 * PendSV handler — does the actual context switch.
 *
 * This is a naked function because we manage the stack ourselves.
 * Same register save/restore as our cooperative sched_context_switch,
 * but triggered by hardware (exception entry) instead of a function call.
 *
 * On exception entry, the CPU automatically pushes:
 *   r0, r1, r2, r3, r12, lr, pc, xPSR  (8 words)
 * We save the rest: r4-r11.
 * Then we switch the PSP (process stack pointer) to the next task.
 */
void pendsv_handler(void) __attribute__((naked));
void pendsv_handler(void)
{
    __asm volatile(
        /* Save current task context */
        "mrs   r0, psp             \n"  /* r0 = current task's stack pointer */
        "stmdb r0!, {r4-r11}       \n"  /* push r4-r11 onto task stack */

        /* Save sp to current TCB and pick next task */
        "bl    sched_preempt       \n"  /* r0 = old sp in, returns new sp in r0 */

        /* Restore next task context */
        "ldmia r0!, {r4-r11}       \n"  /* pop r4-r11 from new task stack */
        "msr   psp, r0             \n"  /* set PSP to new task's stack */

        "ldr   r0, =0xFFFFFFFD     \n"  /* EXC_RETURN: return to thread mode, PSP */
        "bx    r0                  \n"
    );
}
