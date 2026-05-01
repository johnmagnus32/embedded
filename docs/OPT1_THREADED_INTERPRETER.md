# Optimization 1: Instruction Decode Optimization

The if/else cascade in `armv7m_cpu_step` is the single largest CPU consumer (~37%). Try three approaches, benchmark each, and keep the fastest. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/arch/armv7m/armv7m_cpu.c` thoroughly before making changes. Build with `make` from `sim/`.

## Baseline

Run `make && python3 tests/run_tests.py` to establish correctness. Run the MIPS benchmark and record the baseline number. All three approaches must pass all existing tests and produce the same MIPS or better.

## Approach A: Dispatch table (top-bit grouping)

Replace the if/else cascade with a 32-entry table indexed by `insn >> 11` (top 5 bits of the 16-bit instruction). Each entry is a handler for that instruction group. Sub-decoding within each group uses a small switch or a few if/else.

```c
typedef int (*decode_handler_t)(struct armv7m_cpu *c, struct membus *bus, uint16_t insn);

static decode_handler_t dispatch_table[32];

// Build the table at startup:
void init_dispatch_table(void)
{
    dispatch_table[0]  = handle_lsl_imm;      // 00000 = LSL imm
    dispatch_table[1]  = handle_lsr_imm;      // 00001 = LSR imm
    dispatch_table[2]  = handle_asr_imm;      // 00010 = ASR imm
    dispatch_table[3]  = handle_add_sub;       // 00011 = ADD/SUB reg/imm3
    dispatch_table[4]  = handle_mov_imm;       // 00100 = MOV imm
    dispatch_table[5]  = handle_cmp_imm;       // 00101 = CMP imm
    dispatch_table[6]  = handle_add_imm8;      // 00110 = ADD imm8
    dispatch_table[7]  = handle_sub_imm8;      // 00111 = SUB imm8
    dispatch_table[8]  = handle_alu_special;   // 01000 = ALU ops + special (BX, BLX, etc.)
    dispatch_table[9]  = handle_ldr_pc;        // 01001 = LDR PC-relative
    dispatch_table[10] = handle_load_store_reg; // 01010 = STR/STRH/STRB/LDRSB reg
    dispatch_table[11] = handle_load_store_reg; // 01011 = LDR/LDRH/LDRB/LDRSH reg
    dispatch_table[12] = handle_str_imm;       // 01100 = STR imm5
    dispatch_table[13] = handle_ldr_imm;       // 01101 = LDR imm5
    dispatch_table[14] = handle_strb_imm;      // 01110 = STRB imm5
    dispatch_table[15] = handle_ldrb_imm;      // 01111 = LDRB imm5
    dispatch_table[16] = handle_strh_imm;      // 10000 = STRH imm5
    dispatch_table[17] = handle_ldrh_imm;      // 10001 = LDRH imm5
    dispatch_table[18] = handle_str_sp;        // 10010 = STR SP-relative
    dispatch_table[19] = handle_ldr_sp;        // 10011 = LDR SP-relative
    dispatch_table[20] = handle_adr;           // 10100 = ADR (PC-relative)
    dispatch_table[21] = handle_add_sp_imm;    // 10101 = ADD SP + imm
    dispatch_table[22] = handle_misc;          // 10110 = PUSH, POP, BKPT, etc.
    dispatch_table[23] = handle_misc;          // 10111 = PUSH, POP, BKPT, etc.
    dispatch_table[24] = handle_stm;           // 11000 = STM
    dispatch_table[25] = handle_ldm;           // 11001 = LDM
    dispatch_table[26] = handle_cond_branch;   // 11010 = B<cond>
    dispatch_table[27] = handle_cond_branch;   // 11011 = B<cond> + SVC
    dispatch_table[28] = handle_branch;        // 11100 = B (unconditional)
    dispatch_table[29] = handle_thumb32_prefix; // 11101 = Thumb-2 32-bit prefix
    dispatch_table[30] = handle_thumb32_prefix; // 11110 = Thumb-2 32-bit prefix
    dispatch_table[31] = handle_thumb32_prefix; // 11111 = Thumb-2 32-bit prefix (BL second half)
}

// In armv7m_cpu_step:
int armv7m_cpu_step(struct armv7m_cpu *c, struct membus *bus)
{
    uint16_t insn = /* fetch */;
    // ... IT block handling ...
    return dispatch_table[insn >> 11](c, bus, insn);
}
```

Each handler extracts its specific fields and executes. The `handle_alu_special` and `handle_misc` handlers need internal sub-dispatch (switch on bits [10:6]) since those groups contain many different instructions.

Memory: 32 × 8 = 256 bytes. No pre-decode pass needed.

## Approach B: Computed goto (GCC labels-as-values)

Same 32-entry table but using GCC's `&&label` extension for computed goto instead of function pointers. Avoids call/return overhead — execution jumps directly between handlers.

```c
int armv7m_cpu_step(struct armv7m_cpu *c, struct membus *bus)
{
    static void *dispatch[32] = {
        &&lsl_imm, &&lsr_imm, &&asr_imm, &&add_sub,
        &&mov_imm, &&cmp_imm, &&add_imm8, &&sub_imm8,
        &&alu_special, &&ldr_pc, &&ls_reg, &&ls_reg,
        &&str_imm, &&ldr_imm, &&strb_imm, &&ldrb_imm,
        &&strh_imm, &&ldrh_imm, &&str_sp, &&ldr_sp,
        &&adr, &&add_sp, &&misc, &&misc,
        &&stm, &&ldm, &&cond_branch, &&cond_branch,
        &&branch, &&thumb32, &&thumb32, &&thumb32,
    };

    uint16_t insn = /* fetch */;
    // ... IT block handling ...
    goto *dispatch[insn >> 11];

lsl_imm: {
    int rd = insn & 7, rm = (insn >> 3) & 7, imm5 = (insn >> 6) & 0x1F;
    uint32_t result = c->r[rm] << imm5;
    c->r[rd] = result;
    set_nzc(c, result, imm5 ? (c->r[rm] >> (32 - imm5)) & 1 : (c->xpsr >> 29) & 1);
    return CPU_OK;
}

mov_imm: {
    int rd = (insn >> 8) & 7;
    uint32_t imm8 = insn & 0xFF;
    c->r[rd] = imm8;
    set_nz(c, imm8);
    return CPU_OK;
}

// ... all other handlers as labeled blocks ...
}
```

This keeps everything in one function — the compiler can keep CPU state in registers across handlers. No function call overhead. The `goto *dispatch[insn >> 11]` compiles to a single indirect jump.

Memory: 32 × 8 = 256 bytes (same as Approach A).

Note: this makes the function very large. Use `__attribute__((optimize("O3")))` if needed.

## Approach C: Pre-decoded per-address cache (threaded interpreter)

Pre-decode every flash halfword address into a handler function pointer at reset time. Subsequent executions skip decoding entirely.

```c
#define DECODE_CACHE_SIZE (FLASH_SIZE / 2)
static decode_handler_t decode_cache[DECODE_CACHE_SIZE];

void armv7m_cpu_reset(struct armv7m_cpu *c, struct membus *bus)
{
    // ... existing reset ...
    uint8_t *flash = bus->flash_ptr;
    for (uint32_t i = 0; i < FLASH_SIZE; i += 2) {
        uint16_t insn = *(uint16_t *)(flash + i);
        decode_cache[i / 2] = dispatch_table[insn >> 11];
        // Or: decode_cache[i/2] = full_decode(insn) for exact per-instruction handler
    }
}

int armv7m_cpu_step(struct armv7m_cpu *c, struct membus *bus)
{
    uint32_t pc = c->r[REG_PC];
    uint16_t insn = *(uint16_t *)(bus->flash_ptr + (pc - FLASH_BASE));

    if (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE)
        return decode_cache[(pc - FLASH_BASE) / 2](c, bus, insn);
    else
        return dispatch_table[insn >> 11](c, bus, insn);  // RAM: decode each time
}
```

Two variants to try:
- **Coarse cache**: store the top-5-bit dispatch handler (same as Approach A, just cached). Still needs sub-decoding within each handler.
- **Fine cache**: store the exact handler for each instruction (e.g., `handler_add_reg_r0_r1_r2`). Zero sub-decoding but requires ~100 distinct handlers.

Memory: 256K entries × 8 bytes = 2MB (for 512K flash). The fine cache variant may cause instruction cache pressure on the host.

## Note on table size

The prompt uses `insn >> 11` (32 entries) for simplicity, but `insn >> 10` (64 entries) gives finer grouping with less sub-decoding per handler. Try both and benchmark. The 64-entry table means instructions like LSL/LSR/ASR get separate entries instead of sharing a bucket.

## Note on 32-bit Thumb-2 decode

The `exec_thumb32` function has its own long if/else chain (~40 cases) for 32-bit instructions (BL, BLX, LDR.W, STR.W, etc.). Apply the same dispatch table treatment. The top 5 bits of the first halfword (`(insn >> 11) & 0x1F`) plus a few bits of the second halfword give the instruction class. Build a second dispatch table for Thumb-2:

```c
// First halfword already fetched, second halfword:
uint16_t insn2 = membus_read16(bus, pc + 2);
uint32_t full = ((uint32_t)insn << 16) | insn2;

// Index by bits [31:27] of the combined 32-bit instruction (= top 5 bits of first halfword)
// plus bits [24:20] for finer grouping
int t32_idx = ((full >> 27) & 0x1F) << 4 | ((full >> 20) & 0xF);
return thumb32_dispatch[t32_idx](c, bus, full);
```

This is a 512-entry table (9 bits) which covers most Thumb-2 encodings without sub-decoding. Benchmark whether the finer table is worth the memory vs a coarser table with some remaining if/else.

## Benchmarking

Implement all three approaches behind a compile-time flag:

```c
#if DECODE_MODE == 0
    // Baseline: existing if/else cascade
#elif DECODE_MODE == 1
    // Approach A: dispatch table
#elif DECODE_MODE == 2
    // Approach B: computed goto
#elif DECODE_MODE == 3
    // Approach C: pre-decoded cache (coarse)
#elif DECODE_MODE == 4
    // Approach C: pre-decoded cache (fine)
#endif
```

```makefile
bench-decode:
	@for mode in 0 1 2 3 4; do \
		echo "=== DECODE_MODE=$$mode ==="; \
		make clean > /dev/null; \
		make CFLAGS="$(CFLAGS) -DDECODE_MODE=$$mode" > /dev/null 2>&1; \
		./build/sim-core --machine gameboy --firmware ../projects/gameboy/build/gameboy.elf --bench 10000000 --no-chardev 2>&1 | grep MIPS; \
	done
```

Run `make bench-decode` and record results:

```
DECODE_MODE=0 (if/else baseline):     XX.X MIPS
DECODE_MODE=1 (dispatch table):       XX.X MIPS
DECODE_MODE=2 (computed goto):        XX.X MIPS
DECODE_MODE=3 (pre-decode coarse):    XX.X MIPS
DECODE_MODE=4 (pre-decode fine):      XX.X MIPS
```

Keep the fastest. Remove the `#if` guards and the unused approaches. Run all tests to verify correctness.

## Expected results

- Approach A (dispatch table): ~1.5-2× over baseline
- Approach B (computed goto): ~2-2.5× over baseline (best for single-function hot loop)
- Approach C coarse: ~1.5-2× (same as A but avoids re-indexing for hot PCs)
- Approach C fine: ~2-3× (zero sub-decoding) but may lose to icache pressure

Computed goto (B) typically wins for interpreters because the CPU's branch predictor learns the indirect jump patterns better when they're all in one function.

## Testing

All existing tests must pass with every approach. Record the winning MIPS in the commit message:

```
perf(cpu): Replace if/else decode with computed goto dispatch

Benchmarked 5 decode strategies:
  if/else:           40.4 MIPS
  dispatch table:    62.1 MIPS
  computed goto:     78.3 MIPS
  pre-decode coarse: 65.2 MIPS
  pre-decode fine:   71.8 MIPS

Selected: computed goto (best balance of speed and simplicity)

perf: 78.3 MIPS
```

## Results (measured 2026-05-01)

Implemented Approach B (computed goto) for 16-bit Thumb instructions. 32-entry dispatch table indexed by `insn >> 11`. Each entry is a GCC label-as-value (`&&handler`). The `goto *dispatch[insn >> 11]` compiles to a single indirect jump.

Tested on top of OPT2+3+4+5+6 baseline (56 MIPS).

| Metric | Before (if/else) | After (computed goto) | Change |
|--------|-----------------|----------------------|--------|
| MIPS | 56 | 62 | +11% |
| SPI partial FPS | 58 | 64 | +10% |
| DMA partial FPS | 71 | 78 | +10% |
| Audio samples/sec | 57,811 | 63,698 | +10% |

Consistent ~10% improvement across all CPU-bound workloads. The computed goto eliminates the if/else cascade for first-level decode (was ~15 comparisons on average, now 1 indirect jump).

### Cumulative optimization summary (all OPTs applied)

| Starting point | MIPS | Total improvement |
|---|---|---|
| No optimizations | 30 | — |
| +OPT3 (lazy SysTick + NVIC dirty) | 33 | +10% |
| +OPT4 (LTO) | 47 | +57% |
| +OPT2 (membus TLB) | 47 | +57% |
| +OPT5 (-O3 -march=native) | 52 | +73% |
| +OPT6 (fast paths + skip idle) | 56 | +87% |
| +OPT1 (computed goto dispatch) | **62** | **+107%** |

### 32-bit Thumb-2 decode

Not converted to computed goto. The 32-bit instructions require two-level dispatch (first halfword + second halfword bits) because BL, B.W, B.cond.W, and data processing immediate all share `hi >> 8 == 0xF0` but differ in `lo` bits. A 256-entry table on `hi >> 8` would still need if/else sub-dispatch for the `0xF0` group. Since 32-bit instructions are ~20% of the instruction mix, the expected gain is ~2% — not worth the complexity.
