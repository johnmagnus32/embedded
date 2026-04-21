/*
 * smp_rp2040.c — RP2040 core 1 boot
 *
 * RP2040 starts with only core 0 running. Core 1 is asleep.
 * To wake it, core 0 writes a launch sequence to the SIO FIFO:
 *   1. Flush the FIFO
 *   2. Write: 0, 0, 1, vector_table, stack_pointer, entry_point
 *   3. Core 1 reads these and jumps to entry_point
 *
 * Core 1 then runs its own sched_start() with its own idle thread.
 */

#include <stdint.h>
#include "config.h"

#if defined(CONFIG_SMP) && defined(CONFIG_CPU_CORTEX_M0PLUS)

#include "sched.h"

/* SIO FIFO registers */
#define SIO_BASE        0xD0000000
#define SIO_FIFO_ST     (*(volatile uint32_t *)(SIO_BASE + 0x050))
#define SIO_FIFO_WR     (*(volatile uint32_t *)(SIO_BASE + 0x054))
#define SIO_FIFO_RD     (*(volatile uint32_t *)(SIO_BASE + 0x058))

#define FIFO_ST_VLD     (1 << 0)  /* FIFO has data to read */
#define FIFO_ST_RDY     (1 << 1)  /* FIFO has room to write */

/* Core 1 stack (separate from task stacks) */
static uint8_t core1_stack[1024] __attribute__((aligned(8)));

/* Core 1 entry: start the scheduler on this core */
static void core1_entry(void)
{
    /* Core 1 runs sched_start() which picks a task and never returns.
     * The scheduler's pick_next() will find tasks not running on core 0. */
    sched_start();
}

static void fifo_drain(void)
{
    while (SIO_FIFO_ST & FIFO_ST_VLD)
        (void)SIO_FIFO_RD;
}

static void fifo_write_blocking(uint32_t val)
{
    while (!(SIO_FIFO_ST & FIFO_ST_RDY))
        ;
    SIO_FIFO_WR = val;
    __asm volatile("sev");  /* signal event to wake core 1 */
}

static uint32_t fifo_read_blocking(void)
{
    while (!(SIO_FIFO_ST & FIFO_ST_VLD))
        __asm volatile("wfe");  /* wait for event */
    return SIO_FIFO_RD;
}

/*
 * Boot core 1. Called from main() on core 0 after creating tasks.
 *
 * The RP2040 boot ROM on core 1 is waiting for a launch command
 * via the FIFO. The protocol is:
 *   Write: 0, 0, 1, vector_table_addr, stack_top, entry_point
 *   Read back each value (core 1 echoes them as acknowledgment)
 */
void smp_boot_core1(void)
{
    extern const uint32_t __vector_table[];  /* from startup_m0plus.s */

    uint32_t cmd_sequence[] = {
        0,
        0,
        1,
        (uint32_t)__vector_table,
        (uint32_t)(core1_stack + sizeof(core1_stack)),
        (uint32_t)core1_entry,
    };

    /* May need multiple attempts */
    int seq = 0;
    do {
        fifo_drain();

        for (seq = 0; seq < 6; seq++) {
            fifo_write_blocking(cmd_sequence[seq]);

            /* For commands 0 and 1, core 1 echoes back */
            if (seq < 2) {
                uint32_t response = fifo_read_blocking();
                if (response != cmd_sequence[seq]) {
                    seq = -1;  /* restart sequence */
                    break;
                }
            }
        }
    } while (seq < 6);

    /* Core 1 is now running core1_entry() */
}

#endif /* CONFIG_SMP && CONFIG_CPU_CORTEX_M0PLUS */
