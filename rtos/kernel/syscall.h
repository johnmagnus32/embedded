/*
 * syscall.h — System call interface
 *
 * Tasks run in unprivileged mode (can't access kernel memory).
 * To use kernel services, they execute SVC which traps to the
 * kernel. The kernel validates arguments and performs the operation.
 *
 * Linux: ~400 syscalls (read, write, open, mmap, fork, ...)
 * Zephyr: ~100 syscalls (k_sem_take, k_msgq_put, k_malloc, ...)
 * Ours: 6 syscalls
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Syscall numbers (passed in r0 before SVC) */
#define SYS_YIELD       0
#define SYS_SLEEP_MS    1
#define SYS_SEM_GIVE    2
#define SYS_SEM_TAKE    3
#define SYS_MUTEX_LOCK  4
#define SYS_MUTEX_UNLOCK 5

/*
 * User-mode syscall wrappers.
 * These execute SVC #0 which traps to the kernel.
 * Arguments in r0-r3, syscall number in r0 (shifted).
 *
 * When CONFIG_USERSPACE is disabled, these call the kernel directly.
 */

#include "config.h"

#ifdef CONFIG_USERSPACE

/* Execute SVC with syscall number in r0, arg1 in r1 */
static inline uint32_t syscall0(uint32_t num)
{
    register uint32_t r0 __asm__("r0") = num;
    __asm volatile("svc #0" : "+r"(r0) :: "memory");
    return r0;
}

static inline uint32_t syscall1(uint32_t num, uint32_t arg1)
{
    register uint32_t r0 __asm__("r0") = num;
    register uint32_t r1 __asm__("r1") = arg1;
    __asm volatile("svc #0" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

/* User-mode wrappers that tasks call */
static inline void sys_yield(void)
{
    syscall0(SYS_YIELD);
}

static inline void sys_sleep_ms(uint32_t ms)
{
    syscall1(SYS_SLEEP_MS, ms);
}

static inline void sys_sem_give(void *sem)
{
    syscall1(SYS_SEM_GIVE, (uint32_t)sem);
}

static inline void sys_sem_take(void *sem)
{
    syscall1(SYS_SEM_TAKE, (uint32_t)sem);
}

static inline void sys_mutex_lock(void *mtx)
{
    syscall1(SYS_MUTEX_LOCK, (uint32_t)mtx);
}

static inline void sys_mutex_unlock(void *mtx)
{
    syscall1(SYS_MUTEX_UNLOCK, (uint32_t)mtx);
}

#else /* !CONFIG_USERSPACE — direct calls, no SVC overhead */

#include "sched.h"
#include "sync.h"

#define sys_yield()          sched_yield()
#define sys_sleep_ms(ms)     sched_sleep_ms(ms)
#define sys_sem_give(s)      sem_give((struct semaphore *)(s))
#define sys_sem_take(s)      sem_take((struct semaphore *)(s))
#define sys_mutex_lock(m)    mutex_lock((struct mutex *)(m))
#define sys_mutex_unlock(m)  mutex_unlock((struct mutex *)(m))

#endif /* CONFIG_USERSPACE */

#endif
