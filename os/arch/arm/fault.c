/*
 * fault.c — Fault handlers for Cortex-M
 *
 * When a task violates MPU permissions (writes outside its stack),
 * the CPU triggers a MemManage fault. We print diagnostic info
 * instead of silently hanging.
 *
 * Zephyr equivalent: arch/arm/core/cortex_m/fault.c
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_MPU

/* Fault status registers */
#define SCB_CFSR    (*(volatile uint32_t *)0xE000ED28)
#define SCB_MMFAR   (*(volatile uint32_t *)0xE000ED34)  /* MemManage fault address */

/* UART for fault output (direct register access — can't use driver in fault) */
#define USART2_SR   (*(volatile uint32_t *)0x40004400)
#define USART2_DR   (*(volatile uint32_t *)0x40004404)

static void fault_putc(char c)
{
    while (!(USART2_SR & (1 << 7)));
    USART2_DR = c;
}

static void fault_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') fault_putc('\r');
        fault_putc(*s++);
    }
}

static void fault_hex(uint32_t val)
{
    fault_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;
        fault_putc(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
    }
}

/*
 * MemManage fault handler.
 * Called when a task accesses memory it doesn't have permission for.
 *
 * The vector table entry for MemManage is at offset 0x10.
 */
void memmanage_handler(void)
{
    uint32_t cfsr = SCB_CFSR;
    uint32_t mmfar = SCB_MMFAR;

    fault_puts("\n\n*** MEMORY FAULT ***\n");

    /* Get current task name */
    extern const char *sched_current_name(void);
    fault_puts("Task: ");
    fault_puts(sched_current_name());
    fault_puts("\n");

    if (cfsr & (1 << 1)) {
        fault_puts("Data access violation at: ");
        fault_hex(mmfar);
        fault_puts("\n");
    }
    if (cfsr & (1 << 0)) {
        fault_puts("Instruction access violation\n");
    }
    if (cfsr & (1 << 3)) {
        fault_puts("Unstacking error\n");
    }
    if (cfsr & (1 << 4)) {
        fault_puts("Stacking error (stack overflow?)\n");
    }

    fault_puts("CFSR: ");
    fault_hex(cfsr);
    fault_puts("\n");

    fault_puts("System halted.\n");
    while (1) __asm volatile("bkpt #0");  /* breakpoint for debugger */
}

#endif /* CONFIG_MPU */
