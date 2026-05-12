# fpga-ppu Makefile
PROJ  = ppu_top
PCF   = icebreaker.pcf
BUILD = build

SRCS  = src/ppu_top.v src/spi_cmd.v src/sprite_table.v src/tile_table.v src/sprite_mem.v src/pixel_gen.v src/lcd_driver.v
LOGIC = src/spi_cmd.v src/sprite_table.v src/tile_table.v src/sprite_mem.v src/pixel_gen.v src/lcd_driver.v
TB    = tb/tb_ppu.v

all: sim netlist schematic

# Simulation (tests pixel_gen + sprite_mem, no hard IP)
sim: $(TB) $(LOGIC)
	@mkdir -p $(BUILD)
	iverilog -o $(BUILD)/sim_ppu -Isrc $(TB) $(LOGIC) sim/sim_cells.v
	vvp $(BUILD)/sim_ppu

wave: sim
	gtkwave $(BUILD)/ppu.vcd

# Gate-level netlist (logic only)
netlist: $(BUILD)/ppu_netlist.json

$(BUILD)/ppu_netlist.json: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(LOGIC); synth_ice40 -top pixel_gen -json $@"

# Synthesis (full design)
synth: $(BUILD)/$(PROJ).bin

$(BUILD)/$(PROJ).json: $(SRCS)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(SRCS); synth_ice40 -top $(PROJ) -json $@"

$(BUILD)/$(PROJ).asc: $(BUILD)/$(PROJ).json $(PCF)
	nextpnr-ice40 --up5k --package sg48 --json $< --pcf $(PCF) --asc $@

$(BUILD)/$(PROJ).bin: $(BUILD)/$(PROJ).asc
	icepack $< $@

prog: $(BUILD)/$(PROJ).bin
	iceprog $<

# Schematics
schematic: $(BUILD)/rtl.svg

$(BUILD)/rtl.svg: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(LOGIC); proc; opt; wreduce; opt -full; clean -purge; show -format svg -prefix $(BUILD)/rtl"

clean:
	rm -rf $(BUILD)

.PHONY: all sim wave synth netlist prog schematic clean
