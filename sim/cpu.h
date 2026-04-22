/*
 * cpu.h — Cortex-M4 CPU emulator
 */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

struct cpu_state {
    uint32_t r[16];
    uint32_t xpsr;
    uint32_t primask;
    uint32_t control;
    uint32_t msp;
    uint32_t psp;
    int running;
    int in_handler;       /* 1 = executing an ISR */
    uint32_t pending_irq; /* bitmask of pending interrupts */
    uint64_t cycle_count;
};

/* Interrupt IDs (bit positions in pending_irq) */
#define IRQ_SYSTICK  (1 << 0)
#define IRQ_PENDSV   (1 << 1)
#define IRQ_SVC      (1 << 2)

/* Memory map */
#define FLASH_BASE  0x08000000
#define FLASH_SIZE  (512 * 1024)
#define RAM_BASE    0x20000000
#define RAM_SIZE    (128 * 1024)

#define USART2_BASE 0x40004400

void cpu_init(struct cpu_state *cpu);
void cpu_reset(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
int  cpu_step(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
void cpu_run(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram, int max_cycles);

#endif
