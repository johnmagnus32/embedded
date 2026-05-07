/*
 * spinlock.h — SMP-safe spinlock
 *
 * On single-core: disabling interrupts is enough (irq_lock).
 * On SMP: the other core is STILL RUNNING with interrupts disabled.
 * We need an atomic test-and-set to synchronize between cores.
 *
 * ARM provides LDREX/STREX (exclusive load/store) for this.
 * Cortex-M0+ doesn't have LDREX — RP2040 provides hardware
 * spinlocks in the SIO block instead.
 *
 * This is why SMP is fundamentally harder than single-core:
 *   Single-core: irq_lock() → safe, nothing else can run
 *   SMP: irq_lock() → safe from THIS core's interrupts,
 *        but core 1 is still executing and can access shared data
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_SMP

/*
 * RP2040 hardware spinlocks (SIO block @ 0xD0000000).
 * 32 hardware spinlock registers at offset 0x100-0x17C.
 * Read: returns 0 if already locked, nonzero if you got it.
 * Write: any write releases the lock.
 *
 * This is a hardware atomic test-and-set — no LDREX needed.
 */
#define SIO_BASE         0xD0000000
#define SIO_SPINLOCK(n)  (*(volatile uint32_t *)(SIO_BASE + 0x100 + (n) * 4))
#define SIO_CPUID        (*(volatile uint32_t *)(SIO_BASE + 0x000))

struct spinlock {
    uint8_t hw_lock_num;  /* which RP2040 hardware spinlock to use (0-31) */
};

#define SPINLOCK_INIT(n) { .hw_lock_num = (n) }

static inline uint32_t spin_lock(struct spinlock *lock)
{
    /* Disable local interrupts first (same as single-core) */
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));

    /* Then spin until we acquire the hardware lock */
    while (SIO_SPINLOCK(lock->hw_lock_num) == 0)
        ;  /* other core holds it — spin */

    return key;
}

static inline void spin_unlock(struct spinlock *lock, uint32_t key)
{
    /* Release hardware lock (any write releases it) */
    SIO_SPINLOCK(lock->hw_lock_num) = 0;

    /* Restore local interrupts */
    __asm volatile("msr primask, %0" :: "r"(key));
}

static inline int get_cpu_id(void)
{
    return SIO_CPUID;  /* 0 or 1 on RP2040 */
}

#else /* !CONFIG_SMP — single core */

struct spinlock {
    uint8_t _unused;
};

#define SPINLOCK_INIT(n) { ._unused = 0 }

/* On single-core, spinlock = just disable interrupts */
static inline uint32_t spin_lock(struct spinlock *lock)
{
    (void)lock;
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));
    return key;
}

static inline void spin_unlock(struct spinlock *lock, uint32_t key)
{
    (void)lock;
    __asm volatile("msr primask, %0" :: "r"(key));
}

static inline int get_cpu_id(void)
{
    return 0;
}

#endif /* CONFIG_SMP */

#endif /* SPINLOCK_H */
