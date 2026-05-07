# fpga-spi Makefile
# `make` builds netlist, schematics, and bitstream
# `make sim` runs testbench, `make prog` flashes hardware

PROJ  = spi_led_top
PCF   = icebreaker.pcf
BUILD = build

TOP   = spi_led_top.v
LOGIC = spi_slave.v
TB    = tb_spi_slave.v

all: netlist schematic synth

# Simulation (Icarus Verilog)
sim: $(TB) $(LOGIC)
	@mkdir -p $(BUILD)
	iverilog -o $(BUILD)/sim_spi $(TB) $(LOGIC)
	vvp $(BUILD)/sim_spi

wave: sim
	gtkwave $(BUILD)/spi_slave.vcd

# Gate-level netlist (logic only, for fpga-sim)
netlist: $(BUILD)/spi_slave_netlist.json

$(BUILD)/spi_slave_netlist.json: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top spi_slave -json $@" $<

# Synthesis (full design with hard IP)
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

# Schematics
schematic: $(BUILD)/rtl.svg $(BUILD)/synth.svg

$(BUILD)/rtl.svg: $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(LOGIC); proc; opt; wreduce; opt -full; clean -purge; show -format svg -prefix $(BUILD)/rtl"

$(BUILD)/synth.svg: $(TOP) $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top $(PROJ); show -format svg -prefix $(BUILD)/synth" $(TOP) $(LOGIC)

clean:
	rm -rf $(BUILD)

.PHONY: all sim wave synth netlist prog schematic clean
