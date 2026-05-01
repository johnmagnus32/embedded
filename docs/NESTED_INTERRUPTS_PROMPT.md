Add nested interrupt support with priority-based preemption to the emulator. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/arch/armv7m/armv7m_nvic.c`, `src/arch/armv7m/armv7m_nvic.h`, `src/arch/armv7m/armv7m_cpu.c`, and `src/arch/armv7m/armv7m_cpu.h` before making changes. Build with `make` from `sim/`.

## Problem

The emulator blocks ALL interrupts while in any handler (`if (cpu->in_handler) return;`). On real Cortex-M4, a higher-priority interrupt can preempt a lower-priority handler. This means a button press (EXTI, high priority) can't preempt the DMA interrupt handler (low priority) that's filling audio samples — the button response is delayed until the DMA handler finishes.

## How real Cortex-M4 handles this

When an interrupt fires while already in a handler:
1. CPU compares pending interrupt's priority with the active handler's priority
2. If pending priority is higher (lower number): push another exception frame onto MSP, jump to new handler
3. When new handler returns (EXC_RETURN): pop frame, resume preempted handler
4. If pending priority is equal or lower: stays pending until current handler returns

Priority values: 0 = highest, 255 = lowest. Thread mode (no handler) is effectively priority 256 — anything can preempt it.

## Changes

### 1. Add priority tracking to CPU state

In `armv7m_cpu.h`:

```c
struct armv7m_cpu {
    /* ... existing fields ... */
    int in_handler;        /* change from boolean to nesting depth */
    int active_priority;   /* priority of currently executing handler, 256 = thread mode */
    int active_vector;     /* vector number of current handler, 0 = thread mode */
};
```

In `armv7m_cpu_init`:
```c
cpu->in_handler = 0;
cpu->active_priority = 256;  /* thread mode — anything can preempt */
cpu->active_vector = 0;
```

### 2. Add NVIC_IPR registers for external interrupt priorities

In `armv7m_nvic.h`:

```c
struct armv7m_nvic {
    /* ... existing fields ... */
    uint8_t ipr[240];     /* interrupt priority registers (one byte per IRQ) */
};
```

Add membus registration for the IPR range (0xE000E400, size 0xF0 = 240 bytes):

```c
// In stm32f411_init:
membus_register(&soc->bus, 0xE000E400, 0xF0, armv7m_ipr_read, armv7m_ipr_write, &soc->nvic);
```

IPR read/write handlers:

```c
uint32_t armv7m_ipr_read(void *opaque, uint32_t offset)
{
    struct armv7m_nvic *n = opaque;
    /* IPR registers are byte-accessible but membus gives us 32-bit reads */
    uint32_t val = 0;
    for (int i = 0; i < 4 && (offset + i) < 240; i++)
        val |= (uint32_t)n->ipr[offset + i] << (i * 8);
    return val;
}

void armv7m_ipr_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct armv7m_nvic *n = opaque;
    for (int i = 0; i < 4 && (offset + i) < 240; i++)
        n->ipr[offset + i] = (val >> (i * 8)) & 0xFF;
}
```

### 3. Add a priority lookup function

```c
/* Get the configured priority for a given vector number.
 * System exceptions (vectors 1-15) use SCB_SHPR registers.
 * External IRQs (vectors 16+) use NVIC_IPR registers.
 * Lower number = higher priority. */
static int get_vector_priority(struct armv7m_nvic *n, int vector)
{
    if (vector == IRQ_VEC_PENDSV) {
        /* PendSV priority is in SCB_SHPR3 bits [23:16] */
        return (n->scb_shpr3 >> 16) & 0xFF;
    }
    if (vector == IRQ_VEC_SYSTICK) {
        /* SysTick priority is in SCB_SHPR3 bits [31:24] */
        return (n->scb_shpr3 >> 24) & 0xFF;
    }
    if (vector == IRQ_VEC_SVC) {
        /* SVC priority is in SCB_SHPR2 bits [31:24] — add SCB_SHPR2 if not present */
        return 0;  /* default to highest for now */
    }
    if (vector >= 16) {
        /* External IRQ: vector 16 = IRQ0, etc. */
        int irq = vector - 16;
        if (irq < 240) return n->ipr[irq];
    }
    return 0;  /* default: highest priority */
}
```

### 4. Update nvic_update for priority-based preemption

Replace the `in_handler` guard with a priority comparison:

```c
void armv7m_nvic_update(struct armv7m_nvic *n, struct armv7m_cpu *cpu, struct membus *bus)
{
    /* Check PENDSVSET */
    if (n->scb_icsr & (1 << 28))
        n->pending |= (1u << IRQ_VEC_PENDSV);

    if (!n->pending || cpu->primask || cpu->irq_shadow)
        return;

    /* Find highest priority pending interrupt */
    int best_vec = 0;
    int best_prio = 256;  /* worst possible */

    for (int v = 2; v < 32; v++) {  /* skip vector 0 (SP) and 1 (Reset) */
        if (!(n->pending & (1u << v))) continue;
        int prio = get_vector_priority(n, v);
        if (prio < best_prio) {
            best_prio = prio;
            best_vec = v;
        }
    }

    /* Only preempt if pending interrupt has strictly higher priority
     * (lower number) than the currently active handler */
    if (best_vec == 0 || best_prio >= cpu->active_priority)
        return;

    /* Take the interrupt */
    n->pending &= ~(1u << best_vec);
    if (best_vec == IRQ_VEC_PENDSV)
        n->scb_icsr &= ~(1 << 28);

    take_interrupt(cpu, bus, best_vec);
    cpu->active_priority = best_prio;
    cpu->active_vector = best_vec;
}
```

### 5. Update take_interrupt for nesting

`take_interrupt` already pushes an exception frame to the current stack. For nested interrupts, it pushes to MSP (handler mode always uses MSP). The existing code should mostly work — just change `in_handler` to a counter:

```c
void take_interrupt(struct armv7m_cpu *c, struct membus *bus, int vector_num)
{
    /* Sync PSP/MSP */
    if ((c->control & 2) && !c->in_handler)
        c->psp = c->r[REG_SP];
    else
        c->msp = c->r[REG_SP];

    /* Push exception frame to current stack */
    uint32_t *sp_ptr;
    if ((c->control & 2) && !c->in_handler)
        sp_ptr = &c->psp;   /* first interrupt: push to PSP */
    else
        sp_ptr = &c->msp;   /* nested or already in handler: push to MSP */

    /* ... existing frame push code ... */

    /* Set EXC_RETURN */
    if (c->in_handler) {
        /* Nested: returning to handler mode, MSP */
        c->r[REG_LR] = 0xFFFFFFF1;
    } else if (c->control & 2) {
        c->r[REG_LR] = 0xFFFFFFFD;  /* return to thread mode, PSP */
    } else {
        c->r[REG_LR] = 0xFFFFFFF9;  /* return to thread mode, MSP */
    }

    /* Switch to handler mode (MSP) */
    c->r[REG_SP] = c->msp;
    c->in_handler++;  /* increment nesting depth */

    /* Jump to vector */
    uint32_t handler = membus_read32(bus, FLASH_BASE + vector_num * 4);
    c->r[REG_PC] = handler & ~1u;
}
```

Note the new EXC_RETURN value `0xFFFFFFF1` — this means "return to handler mode using MSP" (i.e., return to the preempted handler).

### 6. Update exc_return for nesting

```c
static void exc_return(struct armv7m_cpu *c, struct membus *bus, uint32_t exc_ret)
{
    /* Determine which stack to pop from */
    uint32_t *sp_ptr;
    if (exc_ret & 0x4)
        sp_ptr = &c->psp;
    else
        sp_ptr = &c->msp;

    /* ... existing frame pop code ... */

    /* Restore stack pointer */
    if (exc_ret & 0x4) {
        c->r[REG_SP] = c->psp;
        c->control |= 2;
    } else {
        c->r[REG_SP] = c->msp;
    }

    /* Update handler nesting */
    c->in_handler--;

    if (c->in_handler > 0) {
        /* Returned to a preempted handler — restore its priority.
         * The preempted handler's vector/priority was saved implicitly
         * in the exception frame. For simplicity, scan pending+active
         * to determine the new active priority. Or store it explicitly. */
        /* Simple approach: set active_priority to the preempted handler's priority.
         * We need to track this — use a small stack: */
        c->active_priority = c->priority_stack[c->in_handler - 1];
        c->active_vector = c->vector_stack[c->in_handler - 1];
    } else {
        /* Returned to thread mode */
        c->active_priority = 256;
        c->active_vector = 0;
    }

    /* Re-check NVIC since a pending interrupt might now be serviceable */
    /* (handled by nvic_update on the next tick) */
}
```

### 7. Add priority/vector stacks to CPU state

For tracking nested handler priorities:

```c
#define MAX_NEST_DEPTH 8

struct armv7m_cpu {
    /* ... existing fields ... */
    int in_handler;
    int active_priority;
    int active_vector;
    int priority_stack[MAX_NEST_DEPTH];  /* saved priorities for nested handlers */
    int vector_stack[MAX_NEST_DEPTH];
};
```

In `take_interrupt`, before updating active_priority:

```c
if (c->in_handler < MAX_NEST_DEPTH) {
    c->priority_stack[c->in_handler] = c->active_priority;
    c->vector_stack[c->in_handler] = c->active_vector;
}
c->in_handler++;
c->active_priority = get_vector_priority(nvic, vector_num);
c->active_vector = vector_num;
```

Note: `take_interrupt` needs access to the NVIC to look up the priority. Either pass the NVIC pointer or pass the priority value from `nvic_update` which already computed it.

## Testing

Add `tests/firmware/test_nested_irq.c`:

```c
#include "test.h"

static volatile int order[8];
static volatile int order_idx;

/* High priority handler (EXTI0, priority 64) */
void exti0_handler(void) {
    order[order_idx++] = 1;  /* should execute second (preempts PendSV) */
}

/* Low priority handler (PendSV, priority 255) */
void pendsv_handler(void) {
    order[order_idx++] = 2;  /* starts first */

    /* While in PendSV handler, trigger EXTI0 (higher priority) */
    /* EXTI0 should preempt us immediately */
    *(volatile unsigned int *)0xE000E200 = (1 << 6);  /* NVIC_ISPR: set EXTI0 pending */

    /* After EXTI0 returns, we resume here */
    order[order_idx++] = 3;  /* should execute third */
}

void test_main(void)
{
    /* Set PendSV priority to 255 (lowest) */
    *(volatile unsigned int *)0xE000ED20 |= (0xFF << 16);

    /* Set EXTI0 (IRQ6) priority to 64 (higher than PendSV) */
    *(volatile unsigned int *)0xE000E418 = 64;  /* IPR[6] */

    /* Enable EXTI0 in NVIC */
    *(volatile unsigned int *)0xE000E100 = (1 << 6);

    TEST("nested_preemption");
    order_idx = 0;

    /* Trigger PendSV */
    *(volatile unsigned int *)0xE000ED04 = (1 << 28);

    /* Wait for both handlers to complete */
    for (volatile int i = 0; i < 100000 && order_idx < 3; i++) {}

    CHECK(order_idx == 3);
    CHECK(order[0] == 2);  /* PendSV started first */
    CHECK(order[1] == 1);  /* EXTI0 preempted PendSV */
    CHECK(order[2] == 3);  /* PendSV resumed after EXTI0 returned */

    TEST_DONE("nested_irq");
}
```

This test verifies that a high-priority interrupt (EXTI0 at priority 64) preempts a low-priority handler (PendSV at priority 255) and that the preempted handler resumes correctly after the high-priority handler returns.

Also run all existing tests to verify nothing broke — especially `test_irq.c` which tests basic (non-nested) interrupt behavior.
