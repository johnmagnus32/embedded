# PPU Gate-Level Sprite Rendering Bug

## Summary

The PPU renders only background color (no sprite pixels) when simulated at the gate level. Both Icarus Verilog gate-level simulation and our custom gate-level simulator produce the same failure. RTL simulation passes in both Icarus and CXXRTL.

## Reproduction

### RTL (passes)

```bash
cd projects/gameboy-v2
iverilog -o /tmp/sim_rtl -Ifpga tests/fpga/sb_spram_sim.v fpga/*.v tests/fpga/tb_ppu_e2e.v
vvp /tmp/sim_rtl
# → PASS: tb_ppu_e2e
```

### Gate-level (fails)

```bash
# Generate gate-level netlist
yosys -p "read_verilog fpga/spi_cmd.v fpga/sprite_table.v fpga/tile_table.v \
  fpga/sprite_mem.v fpga/pixel_gen.v fpga/lcd_driver.v; \
  read_verilog /tmp/ppu_test_top.v; synth_ice40 -top ppu_test_top; \
  write_verilog /tmp/ppu_gate.v"

# Simulate with Icarus + iCE40 cell models
iverilog -g2012 -o /tmp/sim_gate \
  $OSS_CAD_SUITE/share/yosys/ice40/cells_sim.v \
  /tmp/ppu_gate.v /tmp/tb_gate_ppu.v
vvp /tmp/sim_gate
# → FAIL: total=12474 non_bg=0
```

### Test stimulus (same for both)

1. Upload 2 pixels to SPRAM: `0xF800` (red), `0x07E0` (green)
2. Upload tile table entry: base=0, width_shift=1, height=2
3. Send sprite frame: 1 sprite at position (0, 100), tile 0
4. Monitor LCD_WR rising edges for 500K+ clock cycles
5. Check if any LCD_D byte is not background (0x86, 0x7D, or 0x00)

### Expected result

At least 2-4 non-background bytes on LCD_D when scanline y=100 is rendered (the sprite's red and green pixels).

### Actual result

All LCD output bytes are background color. The sprite is never rendered.

## What works

- LCD driver outputs pixels (LCD_WR toggles correctly, pixel_x and pixel_y advance)
- SPI command decoding works (sprite_wr_en pulses, num_sprites register gets value 1)
- SPRAM write works (pixel data 0xF800/0x07E0 stored correctly)
- Sprite table BRAM write works (sprite entry data present in BRAM memory)
- BRAM read produces correct data when checked directly (RDATA shows non-zero values)
- All RTL testbenches pass (tb_spi_cmd, tb_sprite_table, tb_tile_table, tb_pixel_gen, tb_lcd_driver, tb_ppu_e2e)

## What fails

In the gate-level netlist, `pixel_gen` never finds active sprites during its scanline scan. It goes through the scan loop but concludes `num_active=0` for every scanline, including y=100 where the sprite should be visible.

## Key observation

The `sprite_table` module has a registered read:

```verilog
reg [39:0] rd_data_reg;
always @(posedge clk)
    rd_data_reg <= active_bank ? bank1[rd_idx] : bank0[rd_idx];
assign rd_data = rd_data_reg;
```

In RTL simulation, `bank0[rd_idx]` and `bank1[rd_idx]` are combinational array lookups. The `rd_data_reg` register adds exactly 1 clock cycle of read latency. `pixel_gen` accounts for this with its `S_SCAN_SET` → `S_SCAN_READ` wait state.

After `synth_ice40`, Yosys maps `bank0` and `bank1` to `SB_RAM40_4K` cells. The `SB_RAM40_4K` has its own registered read output (RDATA updates on clock edge).

## Files

- `projects/gameboy-v2/fpga/sprite_table.v` — the module with the registered read
- `projects/gameboy-v2/fpga/pixel_gen.v` — the consumer with wait states
- `projects/gameboy-v2/tests/fpga/tb_ppu_e2e.v` — end-to-end RTL test (passes)
- `/tmp/ppu_test_top.v` — test wrapper with external clock (no SB_HFOSC)
- `sim/src/devices/ice40up5k/` — our gate-level simulator

## Environment

- Yosys 0.38+41
- Icarus Verilog (from oss-cad-suite)
- iCE40 cell models: `$OSS_CAD_SUITE/share/yosys/ice40/cells_sim.v`
