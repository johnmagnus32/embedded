/*
 * armv7m_cpu.h — Cortex-M4 CPU emulator (pure CPU, no devices or debugger)
 */

#ifndef ARMV7M_CPU_H
#define ARMV7M_CPU_H

#include <stdint.h>

#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

struct armv7m_cpu {
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

/* cpu_step return codes */
#define CPU_OK              0
#define CPU_SEMIHOST_EXIT   0x100  /* OR'd with exit code in low byte */
#define CPU_BREAKPOINT      0x200  /* hit BKPT #0 (0xBE00) */

void armv7m_cpu_init(struct armv7m_cpu *cpu);
void armv7m_cpu_reset(struct armv7m_cpu *cpu, struct membus *bus);
int  armv7m_cpu_step(struct armv7m_cpu *cpu, struct membus *bus);
void armv7m_take_interrupt(struct armv7m_cpu *cpu, struct membus *bus, int vector_num);

#endif
