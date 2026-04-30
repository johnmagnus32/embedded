# Optimization 1: Threaded Interpreter for armv7m_cpu_step

The instruction decode if/else chain in `armv7m_cpu_step` is the single largest CPU consumer (~37%). Replace it with a pre-decoded dispatch table. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/arch/armv7m/armv7m_cpu.c` before making changes. Build with `make` from `sim/`.

## Problem

Every instruction walks ~50 if/else comparisons. The CPU's branch predictor can't help because the pattern varies per instruction.

## Solution

Pre-decode each flash address into a handler function pointer on first execution.

### Handler type

```c
typedef void (*insn_handler_t)(struct armv7m_cpu *cpu, struct membus *bus, uint16_t insn);
```

### Decode cache

```c
#define DECODE_CACHE_SIZE (FLASH_SIZE / 2)
static insn_handler_t decode_cache[DECODE_CACHE_SIZE];
```

### Decode function

```c
static insn_handler_t decode_insn(uint16_t insn)
{
    if ((insn & 0xF800) == 0x1800) return handler_add_sub_reg;
    if ((insn & 0xE000) == 0x2000) return handler_mov_cmp_imm;
    // ... all existing patterns ...
    return handler_unknown;
}
```

Same if/else chain but runs only once per unique PC.

### Pre-populate on reset

```c
void armv7m_cpu_reset(struct armv7m_cpu *cpu, struct membus *bus)
{
    // ... existing reset ...
    for (uint32_t i = 0; i < FLASH_SIZE; i += 2) {
        uint16_t insn = *(uint16_t *)(flash + i);
        decode_cache[i / 2] = decode_insn(insn);
    }
}
```

### Replace decode loop

```c
int armv7m_cpu_step(struct armv7m_cpu *cpu, struct membus *bus)
{
    uint32_t pc = cpu->r[REG_PC];
    uint16_t insn = membus_read16(bus, pc);
    if ((insn & 0xFF00) == 0xBE00) return CPU_BREAKPOINT;

    if (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE) {
        decode_cache[(pc - FLASH_BASE) / 2](cpu, bus, insn);  // cached
    } else {
        decode_insn(insn)(cpu, bus, insn);  // slow path for RAM execution
    }
    cpu->cycle_count++;
    return CPU_OK;
}
```

### Extract handlers

Move each if/else branch body into its own function. Each handler implements one instruction class.

For 32-bit Thumb-2 instructions (BL, etc.), the handler for the first halfword fetches the second halfword internally.

## Expected improvement

~2× MIPS (40 → ~80 MIPS).

## Testing

Run `make perf-bench` before and after. Record MIPS in commit message.
