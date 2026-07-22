# Gate-Level Simulator Evaluation Bug

## Status: Partially Fixed

### Fixed
- DFF initialization: all DFFs now start at 0 (matching Icarus `cells_sim.v` behavior). The previous code incorrectly initialized `SB_DFFESS`/`SB_DFFSS` to 1.
- Verilog `pixel_gen.v`: added extra wait states for BRAM read latency (confirmed by Icarus gate-level passing).

### Remaining
Our sim's `pixel_x` counter in `lcd_driver` oscillates between 0 and 1 instead of incrementing to 239. The same netlist works correctly in Icarus gate-level. The bug is in our evaluation engine — likely the counter increment LUT sees stale DFF outputs due to evaluation ordering within a tick.

## Reproduction

### Generate the netlist

```bash
cd projects/gameboy-v2
export PATH="/home/johmagnu/oss-cad-suite/bin:$PATH"
yosys -q -p "read_verilog fpga/spi_cmd.v fpga/sprite_table.v fpga/tile_table.v \
  fpga/sprite_mem.v fpga/pixel_gen.v fpga/lcd_driver.v; \
  read_verilog /tmp/ppu_test_top.v; \
  synth_ice40 -top ppu_test_top -json /tmp/ppu_test_fixed.json"
```

### Icarus gate-level (PASSES)

```bash
yosys -q -p "... synth_ice40 -top ppu_test_top; write_verilog /tmp/ppu_gate_fixed.v"
iverilog -g2012 -o /tmp/sim_gate \
  /home/johmagnu/oss-cad-suite/share/yosys/ice40/cells_sim.v \
  /tmp/ppu_gate_fixed.v /tmp/tb_gate_long.v
timeout 120 vvp /tmp/sim_gate
# → GATE ICARUS: total=47930 non_bg=3 → PASS
```

### Our simulator (FAILS)

```bash
cd sim
gcc -O2 -Isrc/devices/ice40up5k -Isrc/core -Isrc/devices \
  /tmp/test_new_ppu.c src/devices/ice40up5k/ice40up5k.c \
  src/devices/ice40up5k/netlist.c src/devices/ice40up5k/eval.c \
  src/devices/ice40up5k/cJSON.c -o /tmp/test_ppu
/tmp/test_ppu
# → Gate-level sim: total=125000 non_bg=0 → FAIL
```

### Test stimulus (identical for both)

1. Upload 2 pixels to SPRAM (0xF800, 0x07E0)
2. Upload tile table entry (base=0, width_shift=1, height=2)
3. Send sprite frame (1 sprite at y=100, tile 0)
4. Wait, then monitor LCD_WR rising edges for 500K ticks
5. Check if any LCD_D byte is not background (0x86, 0x7D, or 0x00)

## What works in our sim

- LCD driver renders full frames (125,000 WR strobes = 240×320 pixels + init commands)
- pixel_x and pixel_y advance correctly through all scanlines
- SPI command decoding works (sprite_wr_en pulses, num_sprites=1 after swap)
- SPRAM has correct pixel data (0xF800, 0x07E0)
- Sprite table BRAMs have data written to them
- BRAM RDATA produces non-zero values when checked directly
- Topo sort correctly positions BRAM reads before consuming LUTs
- Isolated sprite_table test passes (when synthesized standalone as DFFs)
- Isolated BRAM read/write test passes (simple bram_rw module)

## What fails in our sim

`pixel_gen` never finds active sprites during its scanline scan. When rendering scanline y=100 (where the sprite should be), the scan loop completes with `num_active=0` and outputs background color.

## Netlist characteristics

```
Cells: 2189 (LUT4 + DFF variants + CARRY)
Hard cells: 10 (BRAM × 9, SPRAM × 1)
Nets: 2355
Pins: 15
```

The sprite table is mapped to 6 BRAMs (bank0: 3 BRAMs, bank1: 3 BRAMs). Each BRAM is 16 bits wide. The 40-bit sprite entry is split across 3 BRAMs (16+16+8 bits, padded).

## Evaluation engine structure

```c
void ice40up5k_tick(struct ice40up5k *dev) {
    eval_combinational(s);  // topo-sorted LUTs + BRAM reads (at correct position)
    eval_clock_edge(s);     // DFF capture + hard cell writes
    eval_combinational(s);  // propagate new DFF outputs
}
```

BRAM reads are integrated into `eval_combinational` via negative indices in `eval_order`. They are positioned after address-driving LUTs and before data-consuming LUTs (verified — 0 consumers before, 50 consumers after).

## Key difference from Icarus

Icarus uses event-driven simulation with delta cycles. Our sim uses static topological evaluation with 2 passes per tick. The same netlist, same cell types, same stimulus — different results.

## Files

- Simulator: `sim/src/devices/ice40up5k/eval.c`, `ice40up5k.c`
- Netlist: `/tmp/ppu_test_fixed.json`
- Test wrapper: `/tmp/ppu_test_top.v`
- Our test: `/tmp/test_new_ppu.c`
- Icarus test: `/tmp/tb_gate_long.v`
- Icarus cell models: `/home/johmagnu/oss-cad-suite/share/yosys/ice40/cells_sim.v`
