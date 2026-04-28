/*
 * cpu.h — Cortex-M4 CPU emulator (pure CPU, no devices or debugger)
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
    int in_handler;
    int irq_shadow;
    uint8_t it_state;
    uint64_t cycle_count;
};

/* Memory map */
#define FLASH_BASE  0x08000000
#define FLASH_SIZE  (512 * 1024)
#define RAM_BASE    0x20000000
#define RAM_SIZE    (128 * 1024)
#define USART2_BASE 0x40004400

struct membus;

void cpu_init(struct cpu_state *cpu);
void cpu_reset(struct cpu_state *cpu, struct membus *bus);
int  cpu_step(struct cpu_state *cpu, struct membus *bus);
void take_interrupt(struct cpu_state *cpu, struct membus *bus, int vector_num);

#endif
