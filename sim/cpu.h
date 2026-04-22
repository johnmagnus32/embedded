/*
 * cpu.h — Cortex-M4 CPU emulator
 *
 * Emulates: registers, Thumb instruction execution, NVIC basics.
 * Enough to run our RTOS firmware in a host process.
 */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

/* ARM registers */
#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

struct cpu_state {
    uint32_t r[16];       /* r0-r15 (r13=SP, r14=LR, r15=PC) */
    uint32_t xpsr;        /* flags: N, Z, C, V + Thumb bit */
    uint32_t primask;     /* interrupt mask */
    uint32_t control;     /* privilege + stack select */
    uint32_t msp;         /* main stack pointer */
    uint32_t psp;         /* process stack pointer */
    int running;
    uint64_t cycle_count;
};

/* Memory map */
#define FLASH_BASE  0x08000000
#define FLASH_SIZE  (512 * 1024)
#define RAM_BASE    0x20000000
#define RAM_SIZE    (128 * 1024)

/* Peripheral addresses we intercept */
#define USART2_BASE 0x40004400
#define SYSTICK_BASE 0xE000E010
#define SCB_BASE    0xE000ED00
#define NVIC_BASE   0xE000E100

void cpu_init(struct cpu_state *cpu);
void cpu_reset(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
int  cpu_step(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
void cpu_run(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram, int max_cycles);

#endif
