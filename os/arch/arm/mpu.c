/*
 * mpu.c — ARM Cortex-M MPU (Memory Protection Unit)
 *
 * Protects each task's stack from other tasks. On context switch,
 * the scheduler calls mpu_switch_task() to update which stack
 * region is writable.
 *
 * Cortex-M4 MPU has 8 regions. We use:
 *   Region 0: Flash (read-only, executable)
 *   Region 1: Full RAM (read-write, privileged only — default)
 *   Region 2: Current task's stack (read-write, unprivileged)
 *   Region 3: Kernel data (read-write, privileged only)
 *
 * If a task writes outside its stack → MemManage fault.
 *
 * Zephyr equivalent: arch/arm/core/cortex_m/mpu/arm_mpu.c
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_MPU

/* MPU registers (Cortex-M4) */
#define MPU_TYPE    (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL    (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR     (*(volatile uint32_t *)0xE000ED98)  /* Region Number */
#define MPU_RBAR    (*(volatile uint32_t *)0xE000ED9C)  /* Region Base Address */
#define MPU_RASR    (*(volatile uint32_t *)0xE000EDA0)  /* Region Attribute and Size */

/* SCB for enabling MemManage fault */
#define SCB_SHCSR   (*(volatile uint32_t *)0xE000ED24)

/*
 * RASR (Region Attribute and Size Register) fields:
 *   [28]    XN — execute never
 *   [26:24] AP — access permission
 *              011 = full access (privileged + unprivileged)
 *              001 = privileged only
 *   [18:16] TEX
 *   [5:1]   SIZE — region size = 2^(SIZE+1) bytes
 *   [0]     ENABLE
 *
 * SIZE encoding: 4=32B, 7=256B, 8=512B, 9=1KB, 18=512KB, 19=1MB
 */

#define AP_PRIV_ONLY  (1 << 24)   /* privileged read-write only */
#define AP_FULL       (3 << 24)   /* privileged + unprivileged read-write */
#define AP_RO         (6 << 24)   /* read-only for everyone */
#define XN            (1 << 28)   /* no execute */
#define REGION_ENABLE (1 << 0)

/* Size field for RASR: log2(size) - 1 */
#define SIZE_512B     (8 << 1)
#define SIZE_1KB      (9 << 1)
#define SIZE_128KB    (16 << 1)
#define SIZE_512KB    (18 << 1)
#define SIZE_1MB      (19 << 1)

static void mpu_set_region(uint8_t region, uint32_t base, uint32_t rasr)
{
    MPU_RNR = region;
    MPU_RBAR = base & 0xFFFFFFE0;  /* must be aligned to region size */
    MPU_RASR = rasr;
}

void mpu_init(void)
{
    /* Check MPU exists */
    uint32_t regions = (MPU_TYPE >> 8) & 0xFF;
    if (regions == 0) return;  /* no MPU */

    /* Disable MPU while configuring */
    MPU_CTRL = 0;

    /* Region 0: Flash — read-only, executable */
    mpu_set_region(0, 0x08000000,
        AP_RO | SIZE_512KB | REGION_ENABLE);

    /* Region 1: All RAM — privileged only (default deny for tasks) */
    mpu_set_region(1, 0x20000000,
        AP_PRIV_ONLY | XN | SIZE_128KB | REGION_ENABLE);

    /* Region 2: will be set per-task on context switch */
    mpu_set_region(2, 0, 0);  /* disabled initially */

    /* Enable MemManage fault handler */
    SCB_SHCSR |= (1 << 16);  /* MEMFAULTENA */

    /* Enable MPU: PRIVDEFENA=1 (privileged code can access everything),
     * ENABLE=1 */
    MPU_CTRL = (1 << 2) | (1 << 0);

    /* Memory barrier to ensure MPU config takes effect */
    __asm volatile("dsb\n isb");
}

/*
 * Called on every context switch to update Region 2
 * to the new task's stack area.
 */
void mpu_switch_task(uint32_t stack_base, uint32_t stack_size)
{
    uint8_t size_bits;

    /* Convert stack size to MPU SIZE field */
    if (stack_size <= 256)       size_bits = 7;   /* 256B */
    else if (stack_size <= 512)  size_bits = 8;   /* 512B */
    else if (stack_size <= 1024) size_bits = 9;   /* 1KB */
    else                         size_bits = 10;  /* 2KB */

    MPU_RNR = 2;
    MPU_RBAR = stack_base & 0xFFFFFFE0;
    MPU_RASR = AP_FULL | XN | (size_bits << 1) | REGION_ENABLE;
}

#endif /* CONFIG_MPU */
