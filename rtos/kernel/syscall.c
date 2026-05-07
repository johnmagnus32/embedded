/*
 * syscall.c — SVC handler and syscall dispatch
 *
 * When a task executes SVC #0, the CPU:
 *   1. Pushes r0-r3, r12, lr, pc, xPSR to the task's stack (PSP)
 *   2. Switches to privileged mode + MSP
 *   3. Jumps to svc_handler (vector table offset 0x2C)
 *
 * We read the syscall number from the stacked r0, dispatch to the
 * kernel function, and return the result in stacked r0.
 *
 * This is the same mechanism as Linux's syscall entry:
 *   User: SVC #0 → kernel: sys_call_table[r0](r1, r2, ...)
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_USERSPACE

#include "syscall.h"
#include "sched.h"
#include "sync.h"

/*
 * The SVC handler is called in handler mode (privileged, MSP).
 * The task's registers were pushed to PSP by hardware.
 * We need to read the stacked frame to get syscall args.
 *
 * Stack frame on PSP (pushed by hardware on exception entry):
 *   PSP+0:  r0  ← syscall number (and return value)
 *   PSP+4:  r1  ← arg1
 *   PSP+8:  r2  ← arg2
 *   PSP+12: r3  ← arg3
 *   PSP+16: r12
 *   PSP+20: lr
 *   PSP+24: pc  (return address)
 *   PSP+28: xPSR
 */

/* C handler — called from assembly with pointer to stacked frame */
void svc_dispatch(uint32_t *frame)
{
    uint32_t syscall_num = frame[0];  /* stacked r0 */
    uint32_t arg1 = frame[1];         /* stacked r1 */

    switch (syscall_num) {
    case SYS_YIELD:
        sched_yield();
        break;

    case SYS_SLEEP_MS:
        sched_sleep_ms(arg1);
        break;

    case SYS_SEM_GIVE:
        /* TODO: validate that arg1 points to a real semaphore */
        sem_give((struct semaphore *)arg1);
        break;

    case SYS_SEM_TAKE:
        sem_take((struct semaphore *)arg1);
        break;

    case SYS_MUTEX_LOCK:
        mutex_lock((struct mutex *)arg1);
        break;

    case SYS_MUTEX_UNLOCK:
        mutex_unlock((struct mutex *)arg1);
        break;

    default:
        /* Unknown syscall — could fault or return error */
        frame[0] = (uint32_t)-1;
        return;
    }

    frame[0] = 0;  /* return 0 = success (written back to task's r0) */
}

/*
 * SVC handler entry (assembly).
 * Gets PSP (task's stack pointer), passes it to svc_dispatch.
 */
void svc_handler(void) __attribute__((naked));
void svc_handler(void)
{
    __asm volatile(
        "mrs  r0, psp       \n"  /* r0 = task's stack pointer */
        "b    svc_dispatch   \n"  /* tail-call to C handler */
    );
}

#endif /* CONFIG_USERSPACE */
