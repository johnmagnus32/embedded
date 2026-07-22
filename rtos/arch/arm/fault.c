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

/* Fault status registers (SCB) */
#define SCB_CFSR    (*(volatile uint32_t *)0xE000ED28)
#define SCB_HFSR    (*(volatile uint32_t *)0xE000ED2C)  /* HardFault status */
#define SCB_MMFAR   (*(volatile uint32_t *)0xE000ED34)  /* MemManage fault address */
#define SCB_BFAR    (*(volatile uint32_t *)0xE000ED38)  /* BusFault address */

/* UART for fault output (direct register access — can't use driver in fault).
 * The gameboy console is USART1 (0x40011000); DT_FAULT_UART_BASE can override. */
#ifndef FAULT_UART_BASE
#define FAULT_UART_BASE 0x40011000u   /* USART1 */
#endif
#define FAULT_USART_SR  (*(volatile uint32_t *)(FAULT_UART_BASE + 0x00))
#define FAULT_USART_DR  (*(volatile uint32_t *)(FAULT_UART_BASE + 0x04))

static void fault_putc(char c)
{
    while (!(FAULT_USART_SR & (1 << 7)));
    FAULT_USART_DR = c;
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
 * HardFault handler — the previously-null vector at offset 0x0C.
 *
 * A HardFault is usually an escalated BusFault/UsageFault (their handlers are
 * also unwired). We dump the STACKED exception frame so the faulting PC/LR are
 * visible: the ARM Cortex-M pushes {R0,R1,R2,R3,R12,LR,PC,xPSR} onto the stack
 * that was active when the fault hit (MSP or PSP, selected by EXC_RETURN bit 2).
 *
 * A naked wrapper picks the right stack pointer and passes it to the C handler.
 */
void hardfault_handler_c(uint32_t *frame)
{
    uint32_t cfsr = SCB_CFSR;
    uint32_t hfsr = SCB_HFSR;

    fault_puts("\n\n*** HARD FAULT ***\n");

    extern const char *sched_current_name(void);
    fault_puts("Task: ");
    fault_puts(sched_current_name());
    fault_puts("\n");

    /* Stacked frame: [0]=R0 [1]=R1 [2]=R2 [3]=R3 [4]=R12 [5]=LR [6]=PC [7]=xPSR */
    fault_puts("PC  (faulting instr): "); fault_hex(frame[6]); fault_puts("\n");
    fault_puts("LR  (caller):         "); fault_hex(frame[5]); fault_puts("\n");
    fault_puts("xPSR:                 "); fault_hex(frame[7]); fault_puts("\n");
    fault_puts("R0="); fault_hex(frame[0]);
    fault_puts(" R1="); fault_hex(frame[1]);
    fault_puts(" R2="); fault_hex(frame[2]);
    fault_puts(" R3="); fault_hex(frame[3]); fault_puts("\n");

    fault_puts("CFSR: "); fault_hex(cfsr); fault_puts("\n");
    fault_puts("HFSR: "); fault_hex(hfsr); fault_puts("\n");

    if (cfsr & (1u << 25)) { fault_puts("UsageFault: DIVBYZERO\n"); }
    if (cfsr & (1u << 24)) { fault_puts("UsageFault: UNALIGNED\n"); }
    if (cfsr & (1u << 18)) { fault_puts("UsageFault: INVPC (bad EXC_RETURN/PC)\n"); }
    if (cfsr & (1u << 17)) { fault_puts("UsageFault: INVSTATE (bad Thumb/EPSR)\n"); }
    if (cfsr & (1u << 16)) { fault_puts("UsageFault: UNDEFINSTR\n"); }
    if (cfsr & (1u << 15)) { fault_puts("BusFault addr valid: "); fault_hex(SCB_BFAR); fault_puts("\n"); }
    if (cfsr & (1u << 10)) { fault_puts("BusFault: IMPRECISERR\n"); }
    if (cfsr & (1u << 9))  { fault_puts("BusFault: PRECISERR\n"); }

    fault_puts("System halted.\n");
    while (1) __asm volatile("bkpt #0");
}

/* Naked entry: select MSP/PSP via EXC_RETURN bit 2 in LR, pass frame to C. */
__attribute__((naked)) void hardfault_handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"  /* EXC_RETURN bit 2: 0=MSP, 1=PSP */
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b hardfault_handler_c\n"
    );
}

#ifdef CONFIG_MPU

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
