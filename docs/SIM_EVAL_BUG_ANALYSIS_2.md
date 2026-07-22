# SIM_EVAL_BUG Analysis: BRAM Read Re-execution in Step 3

## Summary

The custom gate-level simulator treats BRAM reads as combinational (re-executing them whenever `eval_combinational` is called), but real `SB_RAM40_4K` has a **registered read output** that only updates on clock edges. The second `eval_combinational` call (Step 3) corrupts BRAM RDATA by re-reading with post-edge addresses.

## The Icarus model (correct behavior)

From `cells_sim.v`:
```verilog
always @(posedge RCLK) begin
    if (RE && RCLKE) begin
        RDATA_I <= memory[RADDR[7:0]] & ~RMASK_I;
    end
end
```

RDATA updates **only on clock edges**. Between edges, it holds its value regardless of RADDR changes. This is a register, not a combinational lookup.

## Our simulator (incorrect behavior)

```c
void ice40up5k_tick(struct ice40up5k *dev) {
    eval_combinational(s);  // Step 1: BRAM reads execute ← correct
    eval_clock_edge(s);     // Step 2: DFFs capture + BRAM writes
    eval_combinational(s);  // Step 3: BRAM reads execute AGAIN ← BUG
}
```

In Step 3, DFFs have just updated (Step 2). New DFF outputs propagate through LUTs to BRAM RADDR pins. Then `eval_combinational` re-executes the BRAM read with the **new** address, overwriting RDATA with data that shouldn't be visible until the next clock edge.

## The `skip_hard_reads` mechanism

The fix already exists in the code but isn't wired up:

- `netlist.h` declares: `int skip_hard_reads;`
- `eval.c` checks: `if (s->skip_hard_reads) continue;`
- `ice40up5k.c` **never sets it to 1**

## Detailed failure trace

The Verilog fix (adding `S_SCAN_WAIT` to `pixel_gen`) accounts for the BRAM's 1-cycle registered read latency. With the 3-phase state machine:

```
Tick N:   pixel_gen sets sprite_rd_idx = 0, enters S_SCAN_SET
Tick N+1: S_SCAN_SET → S_SCAN_WAIT (BRAM latches addr 0 on this edge, RDATA updates)
Tick N+2: S_SCAN_WAIT → S_SCAN_READ (rd_data_reg captures RDATA, pixel_gen reads it)
```

This works in Icarus because RDATA holds its value between edges. But in our sim:

```
Tick N:
  Step 1: BRAM reads whatever address was on RADDR (from previous tick) → RDATA = old data
  Step 2: DFFs update: sprite_rd_idx ← 0, state ← S_SCAN_SET
  Step 3: New sprite_rd_idx=0 propagates to BRAM RADDR
          BRAM read executes: RDATA ← mem[0]  ← PREMATURE!

Tick N+1:
  Step 1: BRAM reads addr 0 again (unchanged) → RDATA = mem[0] (same as Step 3 wrote)
  Step 2: DFFs update: state ← S_SCAN_WAIT, sprite_rd_idx stays 0
          rd_data_reg captures RDATA = mem[0] ← this looks correct so far
  Step 3: No address change, BRAM re-reads addr 0 → RDATA = mem[0] (no harm)

Tick N+2:
  Step 1: BRAM reads addr 0 → RDATA = mem[0]
  Step 2: DFFs update: state ← S_SCAN_READ
          rd_data_reg captures RDATA = mem[0] ← still correct
          BUT ALSO: scan_idx ← 1, sprite_rd_idx ← 1 (the S_SCAN_READ logic fires)
  Step 3: New sprite_rd_idx=1 propagates to BRAM RADDR
          BRAM read executes: RDATA ← mem[1]  ← PREMATURE!
```

Now here's the problem. On Tick N+2, `pixel_gen` is in `S_SCAN_READ` and reads `sprite_rd_data` (which comes from `rd_data_reg`). The `rd_data_reg` DFF captured RDATA in Step 2, which at that point still held mem[0] from Step 1. So the read of sprite 0 is actually correct for this tick.

But the damage happens for the **next** sprite. After `S_SCAN_READ` processes sprite 0, it sets `sprite_rd_idx = 1` and goes back to `S_SCAN_SET`. Step 3 immediately reads mem[1] into RDATA. On the next tick:

```
Tick N+3:
  Step 1: BRAM reads addr 1 → RDATA = mem[1] (same value Step 3 already put there)
  Step 2: state ← S_SCAN_WAIT
          rd_data_reg captures RDATA = mem[1]
          sprite_rd_idx stays 1
  Step 3: No address change, BRAM re-reads addr 1 (no harm)

Tick N+4:
  Step 1: BRAM reads addr 1 → RDATA = mem[1]
  Step 2: state ← S_SCAN_READ
          rd_data_reg captures RDATA = mem[1] ← correct!
```

Wait — this trace shows it working. Let me reconsider what's actually going wrong.

## Revised theory: The issue is with BRAM reads in the topo-sort position

The BRAM reads are positioned in `eval_order` based on their address dependencies. In Step 3, when DFF outputs change, the topo-sorted evaluation propagates those changes through LUTs to BRAM addresses, then the BRAM read executes, then downstream LUTs consume the new RDATA.

The problem is more subtle: **the BRAM read in Step 3 produces a value that downstream combinational logic (in the same Step 3 pass) uses to compute DFF D inputs for the NEXT tick.** This means DFFs see a "preview" of data that shouldn't be available until the next clock edge.

Consider the `rd_data_reg` DFF. Its D input comes from the BRAM RDATA (through a mux). In Step 3:
1. BRAM RADDR gets new address from updated DFFs
2. BRAM read executes → RDATA = mem[new_addr]
3. Mux selects RDATA → feeds into `rd_data_reg`'s D input

On the NEXT tick's Step 2, `rd_data_reg` captures this D input. But this D input was computed using RDATA from the **wrong** clock edge — it should have been the RDATA from the current tick's Step 1 (old address), not Step 3 (new address).

**In Icarus:** RDATA holds the old value between edges. The mux feeds old RDATA to `rd_data_reg`'s D input. On the next edge, `rd_data_reg` captures old RDATA. Then RDATA updates with the new address. Then on the edge after that, `rd_data_reg` captures the new RDATA. This is the correct 2-cycle pipeline.

**In our sim:** Step 3 overwrites RDATA with new-address data. The mux feeds new RDATA to `rd_data_reg`'s D input. On the next tick's Step 2, `rd_data_reg` captures new RDATA — one cycle too early. The pipeline is effectively shortened by 1 cycle.

This means the 3-phase wait state fix (`S_SCAN_SET → S_SCAN_WAIT → S_SCAN_READ`) that accounts for 2-cycle latency in Icarus only sees 1-cycle latency in our sim. The extra wait state becomes unnecessary, and the timing is off by one in the other direction — but the net effect depends on exactly which data is being read and when.

## Why it manifests as "no sprites found"

The off-by-one in the BRAM pipeline means `pixel_gen` reads data that's shifted in time. Depending on the exact sequence:
- It might read uninitialized BRAM entries (zeros)
- It might read the wrong sprite index's data
- The `num_sprites` value (from a separate register, not BRAM) is correct, but the BRAM data doesn't match

If the first sprite read returns zeros: `sp_x = 0`, `sp_y = 0`, check `pixel_y >= 0 && pixel_y < 40` fails for y=100. Sprite not added. All subsequent reads are similarly shifted.

## The fix

Set `skip_hard_reads = 1` before Step 3:

```c
void ice40up5k_tick(struct ice40up5k *dev) {
    eval_combinational(s);      // Step 1: BRAM/SPRAM reads (pre-edge addresses)
    eval_clock_edge(s);         // Step 2: DFFs capture + hard cell writes
    s->skip_hard_reads = 1;
    eval_combinational(s);      // Step 3: propagate DFF Q, but hold RDATA/DATAOUT
    s->skip_hard_reads = 0;
}
```

This makes BRAM RDATA hold its Step 1 value through Step 3, matching the Icarus `cells_sim.v` behavior where RDATA only updates on `posedge RCLK`.

## How to verify

### Quick test

Apply the one-line fix and re-run the failing test:
```bash
/tmp/test_ppu
# Expected: non_bg > 0
```

### Isolated BRAM timing test

Write a minimal test that exposes the timing difference:
1. Create a tiny netlist: 1 BRAM, 1 DFF driving RADDR, 1 DFF capturing RDATA
2. Write known data to BRAM address 0 and 1
3. Toggle the address DFF (0 → 1)
4. Check: RDATA should still show addr 0's data for one more tick after the address changes
5. Our sim (without fix): RDATA shows addr 1's data immediately in Step 3
6. Our sim (with fix): RDATA holds addr 0's data until next tick's Step 1

### Regression

Run the full FPGA test suite after the fix to ensure SPRAM behavior is also correct (same issue applies to `SB_SPRAM256KA` — its DATAOUT is also registered).

## Why the field exists but isn't used

The `skip_hard_reads` field in `netlist.h` and the check in `eval.c` were clearly intended for this purpose. Most likely this was implemented during initial development, then the `ice40up5k_tick` function was refactored (perhaps when the 3-step evaluation was introduced) and the `skip_hard_reads = 1` line was accidentally dropped.

## Impact on SPRAM

The same bug affects SPRAM reads. In the PPU, `sprite_mem` uses SPRAM. After `pixel_gen` sets `sprite_addr` in `S_COMPOSE`, Step 3 would immediately read the SPRAM at the new address and update DATAOUT. On the next tick, `S_SPRAM_WAIT` reads the (prematurely updated) DATAOUT. In this case, the premature read happens to produce the correct value (same address, same data), so SPRAM reads appear to work. But if `sprite_addr` changed between Step 3 and the next Step 1 (which it doesn't in this design), it would fail.

The fix (`skip_hard_reads`) is correct for both BRAM and SPRAM.
