# PPU Gate-Level Bug: Root Cause Analysis

## The Bug

The full PPU gate-level netlist renders only background color — no sprites. The RTL simulation passes all tests. The isolated sprite_table gate-level test also passes. Only the combined PPU netlist fails.

## Root Cause

Yosys's `synth_ice40` inserts **extra DFF stages** on some (but not all) BRAM RDATA output bits. This creates inconsistent read latency: some bits of `sprite_rd_data` arrive 1 cycle after the address is presented, others arrive 2 cycles later.

### Evidence

Tracing the netlist from BRAM RDATA to the `rd_data_reg` DFFs:

```
bank0.0.2 RDATA[14] (net 2020) ──────────────────────→ LUT → rd_data_reg DFF
                                                         ↑
bank1.0.2 RDATA[14] (net 2052) ──────────────────────→ LUT ┘
                                                              (1 cycle total: BRAM → LUT → DFF)

bank0.0.0 RDATA[?] → DFF (bank0.0.0_RDATA_SB_DFF_Q) → LUT → rd_data_reg DFF
                                                              (2 cycles total: BRAM → DFF → LUT → DFF)

bank1.0.0 RDATA[?] → DFF (bank1.0.0_RDATA_SB_DFF_Q) → LUT → rd_data_reg DFF
                                                              (2 cycles total)
```

Some bits have 1 cycle of latency (BRAM registered output → mux LUT → rd_data_reg). Other bits have 2 cycles (BRAM → extra DFF → mux LUT → rd_data_reg). The `pixel_gen` state machine has exactly 1 wait state (S_SCAN_SET), which is correct for 1-cycle latency but insufficient for 2-cycle latency.

The result: when pixel_gen reads `sprite_rd_data` in S_SCAN_READ, some bits have the correct value (from the current address) and others have a stale value (from the previous address or zero). The sprite scan sees corrupted data and fails to match any sprite to the current scanline.

## Why Yosys Does This

Yosys's `synth_ice40` pass includes a retiming/pipelining optimization. When it maps a Verilog array to `SB_RAM40_4K`, the BRAM's registered output (RDATA) is one pipeline stage. If the combinational path from RDATA to the next DFF (rd_data_reg) is too long (too many LUTs in series), Yosys may insert an additional register to break the critical path.

This is a **timing optimization for place-and-route**. The iCE40UP5K has a maximum clock frequency determined by the longest combinational path between any two registers. By adding pipeline stages, Yosys reduces the longest path and allows the design to run at a higher clock frequency.

The decision is made per-bit: if one RDATA bit feeds through a deep LUT chain (e.g., the bank-select mux + additional decode logic), Yosys adds a register. If another bit has a shorter path, it doesn't. This creates the inconsistent latency.

### When this happens

- Large designs with many LUTs between BRAM output and the consuming register
- Designs where the BRAM output fans out to multiple consumers through different-depth logic
- When `synth_ice40` determines the timing won't meet the target frequency without pipelining

### When this does NOT happen

- Small/isolated netlists (like `test_sprite_table_gate`) where the path is short
- When the BRAM output goes directly to a register with minimal combinational logic

This explains why the isolated test passes (short path, no extra DFF) but the combined netlist fails (longer path due to more logic, extra DFF inserted).

## Does This Happen on Real Hardware?

**Yes — but it doesn't cause a bug on real hardware.** On real hardware:

1. The extra DFF is a real register that adds 1 clock cycle of latency
2. The design runs at the clock frequency that Yosys/nextpnr determined is safe
3. All bits arrive at the `rd_data_reg` DFF at the same time (the extra DFF just means the data is from 2 cycles ago instead of 1)
4. The `pixel_gen` state machine would need to account for this extra latency — but since Yosys added the DFF, the RTL behavior no longer matches the gate-level behavior

**This is a real hardware bug too**, not just a sim artifact. If you flash this bitstream to a real iCE40, the sprite scan would also fail because `pixel_gen` only waits 1 cycle but some bits need 2.

The RTL simulation passes because it doesn't have the extra DFF — the Verilog source has exactly 1 register in the read path (`rd_data_reg`). The gate-level netlist has 2 registers for some bits.

## Fix Options

### Option 1: Prevent Yosys from adding pipeline registers

```bash
yosys -p "read_verilog ...; synth_ice40 -noretime -top ppu_test_top -json out.json"
```

The `-noretime` flag disables register retiming/pipelining. Yosys will not insert extra DFFs.

**Consequences:**
- The design may not meet timing at higher clock frequencies
- For our 24-48MHz FPGA clock, this is likely fine (iCE40UP5K can easily do 48MHz for simple designs)
- The gate-level netlist will match the RTL behavior exactly
- Safe for our use case

### Option 2: Add an extra wait state in pixel_gen

Add `S_SCAN_WAIT` between `S_SCAN_SET` and `S_SCAN_READ`:

```verilog
S_SCAN_SET: state <= S_SCAN_WAIT;
S_SCAN_WAIT: state <= S_SCAN_READ;  // extra cycle for pipelined BRAM
S_SCAN_READ: // data now valid
```

**Consequences:**
- Sprite scan takes 3 cycles per entry instead of 2
- Rendering is slower (more cycles per pixel on new scanlines)
- Works regardless of Yosys optimization level
- RTL and gate-level behavior match (RTL just has an unnecessary extra wait)

### Option 3: Use `(* keep *)` or `(* syn_preserve *)` attributes

```verilog
(* keep *) reg [39:0] rd_data_reg;
```

This tells Yosys not to optimize the register or add pipeline stages around it.

**Consequences:**
- May or may not prevent the extra DFF (depends on Yosys version)
- Less portable than `-noretime`

## Recommendation

Use **Option 1** (`-noretime`) for synthesis. It's the simplest fix, doesn't change the Verilog, and is safe for our clock frequency. If timing becomes an issue later (unlikely at 48MHz), switch to Option 2.

Verify by re-synthesizing with `-noretime` and checking that the BRAM RDATA nets have consumers (no orphaned nets).
