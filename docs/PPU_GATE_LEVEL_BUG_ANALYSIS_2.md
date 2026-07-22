# PPU Gate-Level Bug Analysis: Double-Registered BRAM Read Latency

## Root Cause

The `sprite_table` read path has **2 cycles of latency** after synthesis but `pixel_gen` only waits **1 cycle**. This causes `pixel_gen` to read stale/uninitialized data during its sprite scan, resulting in zero active sprites and background-only output.

## The Latency Mismatch

### RTL (1 cycle — what pixel_gen expects)

```verilog
// sprite_table.v
always @(posedge clk)
    rd_data_reg <= active_bank ? bank1[rd_idx] : bank0[rd_idx];
assign rd_data = rd_data_reg;
```

In RTL simulation, `bank0[rd_idx]` is a combinational array lookup. The only register is `rd_data_reg`. Timeline:

```
Tick N:   pixel_gen sets sprite_rd_idx = 0 (DFF update)
Tick N+1: rd_data_reg captures bank[0] (combinational lookup + DFF)
          pixel_gen reads rd_data in S_SCAN_READ ← DATA VALID ✓
```

**Total latency: 1 clock cycle.** `pixel_gen`'s `S_SCAN_SET → S_SCAN_READ` accounts for this.

### Gate-level (2 cycles — what actually happens after synth_ice40)

Yosys infers `SB_RAM40_4K` for `bank0` and `bank1`. The `SB_RAM40_4K` has its own **registered read output** — RDATA updates on the clock edge when RE and RCLKE are high. This is one register stage *inside* the BRAM primitive.

The original `rd_data_reg` is a second register stage *outside* the BRAM. Yosys cannot absorb it into the BRAM because the `active_bank ? bank1[rd_idx] : bank0[rd_idx]` mux sits between the two BRAMs' RDATA outputs and the `rd_data_reg` DFF. The post-synthesis structure is:

```
rd_idx → [BRAM RADDR] → (clock edge) → [BRAM RDATA] → [mux] → [rd_data_reg DFF] → rd_data
                              ↑                                        ↑
                         register 1                               register 2
```

Timeline:

```
Tick N:   pixel_gen sets sprite_rd_idx = 0 (DFF update)
          New rd_idx propagates to BRAM RADDR nets
Tick N+1: BRAM clock edge → RDATA updates with bank[0] contents
          rd_data_reg captures RDATA (but RDATA just changed THIS edge,
          so rd_data_reg captures the PREVIOUS RDATA value)
Tick N+2: rd_data_reg now has the correct value from tick N+1's BRAM read
          pixel_gen reads rd_data in S_SCAN_READ ← but this is tick N+1!
```

Wait — let me be more precise about the DFF capture semantics. On a clock edge, all DFFs sample their D inputs simultaneously (the values from *before* the edge). So:

```
Tick N, rising edge:
  - pixel_gen DFF: sprite_rd_idx ← 0 (was set in previous cycle's logic)
  - BRAM: RDATA ← mem[old_RADDR] (reads address from BEFORE this edge)
  - rd_data_reg ← old RDATA (captures RDATA from BEFORE this edge)

After tick N settles:
  - sprite_rd_idx = 0 propagates to BRAM RADDR

Tick N+1, rising edge:
  - BRAM: RDATA ← mem[0] (now reads the correct address)
  - rd_data_reg ← old RDATA (still the stale value — RDATA hasn't updated yet)

After tick N+1 settles:
  - RDATA has mem[0], propagates through mux to rd_data_reg's D input

Tick N+2, rising edge:
  - rd_data_reg ← RDATA = mem[0] ← FINALLY CORRECT

After tick N+2 settles:
  - rd_data = mem[0], visible to pixel_gen
```

**Total latency: 2 clock cycles.** `pixel_gen` only waits 1.

## How this manifests in the custom sim

The custom sim's tick sequence:

```c
void ice40up5k_tick(struct ice40up5k *dev) {
    // Step 1: eval_combinational — BRAM reads execute (using current RADDR)
    eval_combinational(s);

    // Step 2: eval_clock_edge — DFFs capture (including rd_data_reg)
    eval_clock_edge(s);

    // Step 3: eval_combinational (skip hard reads) — propagate new DFF Q values
    s->skip_hard_reads = 1;
    eval_combinational(s);
    s->skip_hard_reads = 0;
}
```

Trace:

```
Tick N:
  Step 1: BRAM reads using whatever RADDR was set by previous tick's Step 3
  Step 2: rd_data_reg DFF captures current RDATA (from Step 1)
          pixel_gen DFFs update: sprite_rd_idx ← 0, state ← S_SCAN_SET
  Step 3: New sprite_rd_idx=0 propagates to BRAM RADDR nets

Tick N+1:
  Step 1: BRAM reads address 0 → RDATA nets get mem[0]
  Step 2: rd_data_reg captures RDATA = mem[0] ← CORRECT VALUE CAPTURED
          pixel_gen state ← S_SCAN_READ
  Step 3: rd_data = rd_data_reg = mem[0] propagates to pixel_gen's sprite_rd_data

Tick N+2:
  pixel_gen is now in S_SCAN_READ and can see the data
```

So `pixel_gen` transitions: `S_IDLE → S_SCAN_SET` (tick N) → `S_SCAN_READ` (tick N+1). But the data isn't valid until tick N+2's combinational propagation. **pixel_gen reads one tick too early.**

## Why the scan always fails

When `pixel_gen` reads `sprite_rd_data` in `S_SCAN_READ`, it's seeing the *previous* BRAM output (whatever was left over from the last read, or zeros from initialization). For the first sprite scan:

- `sprite_rd_data` = 0 (uninitialized)
- `sp_x` = 0, `sp_y` = 0 (extracted from zeros)
- Check: `pixel_y >= 0 && pixel_y < 0 + 40`
- For scanline y=100: **fails** → sprite not added to active list
- Result: `num_active = 0` → output background color

Every subsequent read is similarly off-by-one: when reading sprite N, you get sprite N-1's data (or zeros for the first read).

## Why RTL passes

In RTL, `bank0[rd_idx]` is a combinational lookup — no BRAM primitive, no internal register. The only latency is the explicit `rd_data_reg`. So 1 wait state is sufficient.

## Fixes

### Option A: Add a wait state in pixel_gen (minimal change)

```verilog
localparam S_IDLE = 0, S_SCAN_SET = 1, S_SCAN_WAIT = 2, S_SCAN_READ = 3, ...

S_SCAN_SET: begin
    state <= S_SCAN_WAIT;
end

S_SCAN_WAIT: begin
    state <= S_SCAN_READ;
end
```

Same fix needed for `S_TILE_SET → S_TILE_READ` and `S_SPRAM_SET → S_SPRAM_WAIT` if those also go through double-registered paths.

**Pro:** Minimal code change, works with any synthesis tool.
**Con:** Adds 1 cycle per sprite during scan (8 sprites × 1 extra cycle = 8 cycles per scanline). Negligible impact on frame timing.

### Option B: Remove rd_data_reg from sprite_table (let BRAM be the only register)

```verilog
// Replace registered read with direct BRAM output
// Requires restructuring so Yosys infers the mux BEFORE the BRAM
wire [39:0] bank0_rd, bank1_rd;
// ... instantiate two BRAMs with separate read ports ...
assign rd_data = active_bank ? bank1_rd : bank0_rd;
```

**Pro:** 1-cycle latency matches pixel_gen's expectations, no state machine changes.
**Con:** Requires manual BRAM instantiation or careful coding to ensure Yosys absorbs the output register. The `active_bank` mux complicates inference.

### Option C: Explicit SB_RAM40_4K instantiation

Replace the behavioral `reg [39:0] bank0 [0:63]` with explicit `SB_RAM40_4K` instances. Control RE/RCLKE directly. Use the BRAM's registered output as the only latency stage (no `rd_data_reg`).

**Pro:** Full control over timing, guaranteed 1-cycle latency.
**Con:** Verbose, less portable, harder to read.

## Recommendation

**Option A** is the safest fix. It makes `pixel_gen` correct for both RTL and gate-level simulation. The extra cycle cost is negligible. The same pattern (extra wait state) should be applied to all registered memory reads in `pixel_gen`:

1. `S_SCAN_SET → S_SCAN_WAIT → S_SCAN_READ` (sprite table BRAM)
2. `S_TILE_SET → S_TILE_WAIT → S_TILE_READ` (tile table BRAM)
3. `S_SPRAM_SET → S_SPRAM_WAIT` (SPRAM — check if this also has double latency)

For SPRAM: `SB_SPRAM256KA` has a registered read output but `sprite_mem.v` doesn't add another register on top, so SPRAM should be fine with 1 wait state. Verify by checking if `sprite_mem.v` has an output register.

## Verification

After fixing, both should pass:
```bash
# RTL
iverilog -o /tmp/sim_rtl ... && vvp /tmp/sim_rtl  # still passes

# Gate-level
yosys ... -json /tmp/ppu_gate.json
./sim-core --machine fpga-test --firmware test.elf --device fpga0=/tmp/ppu_gate.json  # now passes
```

The RTL test still passes because the extra wait state just adds a harmless extra cycle — the data was already valid after 1 cycle in RTL, and reading it after 2 cycles is fine (it doesn't change).
