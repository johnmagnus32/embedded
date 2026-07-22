# SIM_EVAL_BUG Analysis

## Context

The Icarus gate-level simulation now passes after two changes:
1. `pixel_gen.v` was updated to use 3-phase memory reads (SET → WAIT → READ) instead of 2-phase (SET → READ)
2. The `sprite_table.v` has an explicit `rd_data_reg` (registered read output)

The Icarus gate-level test passes because the `SB_RAM40_4K` cell model in `cells_sim.v` correctly models the registered RDATA output. Our custom simulator still fails.

## The Timing Model Difference

### Real iCE40 SB_RAM40_4K behavior (and Icarus cell model)

```
Tick N:   Address presented on RADDR
          (RDATA still shows data from previous address)
Tick N+1: RDATA updates with data for the address from tick N
          (registered output — latches on clock edge)
```

The BRAM read is **synchronous** — RDATA only changes on clock edges, one cycle after the address is set.

### Our simulator's current behavior

```c
eval_combinational(s);  // includes BRAM reads — RDATA updates IMMEDIATELY
eval_clock_edge(s);     // DFFs capture
eval_combinational(s);  // propagate new state
```

We treat BRAM reads as **combinational** — RDATA updates in the same tick the address is set. This is wrong. The real `SB_RAM40_4K` has a registered output.

### The consequence

With our model:
```
Tick N:   pixel_gen sets sprite_rd_idx (in S_SCAN_SET or S_IDLE)
          eval_combinational: BRAM immediately outputs data for new idx
          eval_clock_edge: rd_data_reg DFF captures the BRAM output
Tick N+1: eval_combinational: BRAM reads again (same idx, same data)
          pixel_gen in S_SCAN_WAIT — just waiting
Tick N+2: pixel_gen in S_SCAN_READ — reads rd_data_reg (captured on tick N)
```

This seems like it should work (rd_data_reg captured correct data on tick N). But the issue is more subtle:

With the real BRAM model:
```
Tick N:   pixel_gen sets sprite_rd_idx
          BRAM RDATA still shows OLD data (from previous address)
          rd_data_reg captures OLD RDATA (wrong data!)
Tick N+1: BRAM RDATA updates to show data for idx set on tick N
          rd_data_reg captures CORRECT RDATA
Tick N+2: pixel_gen reads rd_data_reg (captured on tick N+1 — correct!)
```

The 3-phase wait (SET → WAIT → READ) accounts for:
- Tick N: set address
- Tick N+1: BRAM output updates (1 cycle latency)
- Tick N+2: rd_data_reg captures BRAM output
- Tick N+3 (S_SCAN_READ): pixel_gen reads rd_data_reg

But in our sim, the BRAM output updates IMMEDIATELY (tick N), so rd_data_reg captures on tick N (too early). Then by tick N+2 when pixel_gen reads, rd_data_reg has data from tick N — which in our model IS correct (because our BRAM responded immediately). So why does it fail?

## The Real Issue

The problem is that our BRAM read happens in `eval_combinational` which runs TWICE per tick (before and after clock_edge). The sequence is:

```
Tick N:
  1. eval_combinational: BRAM reads with CURRENT address (from previous tick's DFF outputs)
     → RDATA = data for previous address (pixel_gen hasn't set new idx yet)
  2. eval_clock_edge: DFFs capture
     → rd_data_reg captures RDATA from step 1 (previous address data)
     → pixel_gen state advances (e.g., S_IDLE → S_SCAN_SET, sets sprite_rd_idx)
  3. eval_combinational: BRAM reads with NEW address (just set by pixel_gen in step 2)
     → RDATA = data for new address
     (but rd_data_reg already captured in step 2 — too late!)
```

The BRAM read in step 3 produces the correct data, but `rd_data_reg` already captured in step 2 using the step 1 data (wrong address). The correct data won't be captured until the NEXT tick's step 2.

This means our sim effectively has **2 cycles of BRAM read latency** (same as real hardware!) — but the timing is shifted. The data captured by `rd_data_reg` is always from the PREVIOUS tick's address, not the current tick's.

Wait — that's actually correct behavior! The real BRAM also has this: rd_data_reg captures RDATA which is from the previous tick's address. So our sim SHOULD produce the same result as Icarus.

## Hypothesis: The issue is in the FIRST eval_combinational

The first `eval_combinational` (before clock_edge) reads the BRAM with the address from the PREVIOUS tick. This is correct. But if the BRAM's internal state was modified by `eval_clock_edge` on the previous tick (a write happened), the data might be different.

More specifically: `eval_clock_edge` calls `eval_hard_cells_write` which modifies the BRAM memory array. Then on the NEXT tick, `eval_combinational` reads from the modified array. This is correct — writes take effect on the next read.

But there's a subtlety: **the BRAM write and read share the same address mux** (`addr_mux = word_we ? word_addr : rd_addr`). If `word_we` is still 1 from the previous tick (because the DFF hasn't been cleared yet), the address mux selects the WRITE address instead of the READ address. Our first `eval_combinational` would read from the wrong address.

However, `word_we` is cleared by `eval_clock_edge` (the DFF captures D=0). So on the next tick's first `eval_combinational`, `word_we` should be 0 and the read address should be used. Unless `word_we` is cleared in step 2 but the first `eval_combinational` (step 1) still sees it as 1.

**That's the bug!** In step 1, `word_we` is still 1 (from the previous tick — it hasn't been cleared yet because clock_edge hasn't run). The BRAM address mux selects `word_addr` instead of `rd_addr`. The BRAM reads from the wrong address. Then in step 2, `word_we` gets cleared. But the damage is done — `rd_data_reg` captured garbage.

## How to verify

1. Check if `word_we` (the SPRAM write enable from `sprite_mem`) is ever 1 during the sprite scan phase
2. If yes, the BRAM address mux is corrupted during that tick
3. The fix: ensure BRAM reads use `rd_addr` even when `word_we` is 1 — or ensure `word_we` is never 1 during rendering

Actually, `word_we` is for the SPRAM (sprite_mem), not the sprite_table BRAMs. The sprite_table BRAMs have their own address mux controlled by `active_bank`. Let me reconsider.

## Alternative hypothesis: BRAM read timing mismatch

The real issue might be simpler. Our sim does:
```
eval_combinational → BRAM reads (outputs RDATA immediately)
eval_clock_edge → rd_data_reg captures RDATA
```

Icarus does:
```
clock edge → BRAM latches address, outputs RDATA (registered)
clock edge → rd_data_reg captures PREVIOUS RDATA (not current!)
next clock edge → rd_data_reg captures the RDATA that was output on this edge
```

In Icarus, `rd_data_reg` is always ONE CYCLE BEHIND the BRAM output. In our sim, `rd_data_reg` captures the BRAM output from the SAME tick (because BRAM read is in step 1, DFF capture is in step 2).

This means our sim has **1 cycle less latency** than real hardware for the BRAM read path. The 3-phase wait in pixel_gen (SET → WAIT → READ) accounts for 2 cycles of latency. Our sim only has 1 cycle. So pixel_gen reads `rd_data_reg` one cycle too early — it gets data from the PREVIOUS sprite, not the current one.

## Proposed fix

Move BRAM reads from `eval_combinational` into `eval_clock_edge` (treat them as synchronous, like DFFs). The BRAM captures the address on the clock edge and outputs data that's available for the NEXT tick's `eval_combinational`.

```c
void eval_clock_edge(struct sim_state *s) {
    // Phase 1: read all DFF D inputs (from current nets)
    // Phase 2: read BRAM addresses, compute BRAM outputs (synchronous read)
    // Phase 3: write all DFF Q outputs + BRAM RDATA to nets
    // Phase 4: hard cell writes (BRAM/SPRAM memory updates)
}
```

This makes BRAM RDATA behave like a DFF output — it updates on the clock edge based on the address from the previous combinational settle. The data is then available in the next `eval_combinational` pass for consuming LUTs/DFFs.

## How to test

1. Make the fix
2. Re-run the PPU gate-level test (`/tmp/test_new_ppu.c`)
3. Verify `non_bg > 0`
4. Run all 55 sim tests to check for regressions
5. Run the isolated BRAM test (`test_fpga_bram`) to verify basic BRAM read/write still works

## Files to modify

- `sim/src/devices/ice40up5k/eval.c` — move BRAM/SPRAM read logic from `eval_combinational` into `eval_clock_edge`
- `sim/src/devices/ice40up5k/ice40up5k.c` — may need to adjust tick structure
- `sim/src/devices/ice40up5k/netlist.c` — remove BRAM reads from topo sort (they're no longer combinational)
