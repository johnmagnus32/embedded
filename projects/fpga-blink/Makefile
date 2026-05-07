# fpga-blink Makefile
# `make` builds netlist, schematics, and bitstream
# `make sim` runs testbench, `make prog` flashes hardware

PROJ  = blink
PCF   = icebreaker.pcf
BUILD = build

# Source files
TOP   = $(PROJ).v       # top-level (hard IP wrapper, synthesis only)
LOGIC = counter.v       # pure logic (testable)
TB    = tb_$(PROJ).v    # testbench (simulation only)

# Default: build everything
all: netlist schematic synth

# Simulation (Icarus Verilog) — tests the logic module, no hard IP
sim: $(TB) $(LOGIC)
	@mkdir -p $(BUILD)
	iverilog -o $(BUILD)/sim_$(PROJ) $(TB) $(LOGIC)
	vvp $(BUILD)/sim_$(PROJ)

wave: sim
	gtkwave $(BUILD)/$(PROJ).vcd

# Gate-level netlist for fpga-sim
netlist: $(BUILD)/counter_netlist.json

$(BUILD)/counter_netlist.json: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top counter -json $@" $<

# Synthesis (Yosys + nextpnr) — includes hard IP wrapper + logic
synth: $(BUILD)/$(PROJ).bin

$(BUILD)/$(PROJ).json: $(TOP) $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top $(PROJ) -json $@" $(TOP) $(LOGIC)

$(BUILD)/$(PROJ).asc: $(BUILD)/$(PROJ).json $(PCF)
	nextpnr-ice40 --up5k --package sg48 --json $< --pcf $(PCF) --asc $@

$(BUILD)/$(PROJ).bin: $(BUILD)/$(PROJ).asc
	icepack $< $@

# Program the iCEBreaker
prog: $(BUILD)/$(PROJ).bin
	iceprog $<

# Schematics (SVG for viewing in browser)
schematic: $(BUILD)/rtl.svg $(BUILD)/synth.svg

$(BUILD)/rtl.svg: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(LOGIC); proc; opt; wreduce; opt -full; clean -purge; show -format svg -prefix $(BUILD)/rtl"

$(BUILD)/synth.svg: $(TOP) $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top $(PROJ); show -format svg -prefix $(BUILD)/synth" $(TOP) $(LOGIC)

# Clean
clean:
	rm -rf $(BUILD)

.PHONY: all sim wave synth netlist prog schematic clean
