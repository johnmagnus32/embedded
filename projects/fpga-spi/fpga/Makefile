# fpga-spi / FPGA Makefile
#
#   make sim    — run the SPI-slave testbench (no hardware), prints PASS/FAIL
#   make synth  — synthesize + place/route -> ../build/spi_led_top.bin (bitstream)
#   make header — convert ../build/spi_led_top.bin -> ../build/bitstream.h
#   make        — sim + synth + header
#
# All artifacts go to the shared project build dir (../build); this directory
# keeps only source. Target board: ice40-breakout (iCE40UP5K-SG48), configured
# over the config SPI bus by an STM32 at boot. No `make prog` — the bitstream is
# embedded into the STM32 firmware (see ../src) rather than flashed to the FPGA.

PROJ  = spi_led_top
TOP   = spi_led_top        # synthesis top (hard IP wrapper + spi_slave)
LOGIC = spi_slave.v        # pure logic (also used by the testbench)
WRAP  = spi_led_top.v      # hard IP wrapper (SB_HFOSC) + LED regs
PCF   = spi.pcf
TB    = tb_spi_slave.v
# shared project build dir (no inline comment — trailing spaces would corrupt it)
BUILD = ../build

all: sim synth header

# --- Simulation (Icarus Verilog): tests pure logic, no hard IP ---
sim: $(TB) $(LOGIC)
	@mkdir -p $(BUILD)
	iverilog -o $(BUILD)/sim_$(PROJ) $(TB) $(LOGIC)
	cd $(BUILD) && vvp sim_$(PROJ)   # writes spi_slave.vcd here, keeping fpga/ clean

wave: sim
	gtkwave $(BUILD)/spi_slave.vcd

# --- Synthesis (Yosys) + place/route (nextpnr) + pack (icepack) ---
synth: $(BUILD)/$(PROJ).bin

$(BUILD)/$(PROJ).json: $(WRAP) $(LOGIC)
	@mkdir -p $(BUILD)
	yosys -p "synth_ice40 -top $(TOP) -json $@" $(WRAP) $(LOGIC)

$(BUILD)/$(PROJ).asc: $(BUILD)/$(PROJ).json $(PCF)
	nextpnr-ice40 --up5k --package sg48 --json $< --pcf $(PCF) --asc $@

$(BUILD)/$(PROJ).bin: $(BUILD)/$(PROJ).asc
	icepack $< $@
	@echo "Bitstream: $@ ($$(wc -c < $@) bytes)"

# --- Embed the bitstream into a C header the STM32 firmware #includes ---
header: $(BUILD)/bitstream.h

$(BUILD)/bitstream.h: $(BUILD)/$(PROJ).bin
	@printf '/* Auto-generated from %s by `make header`. Do not edit. */\n' $< > $@
	@printf '#ifndef BITSTREAM_H\n#define BITSTREAM_H\n#include <stdint.h>\n\n' >> $@
	@printf 'static const uint8_t fpga_bitstream[] = {\n' >> $@
	@od -An -v -tu1 $< | awk '{for(i=1;i<=NF;i++)printf "0x%02x,",$$i; print ""}' >> $@
	@printf '};\nstatic const uint32_t fpga_bitstream_len = sizeof(fpga_bitstream);\n\n' >> $@
	@printf '#endif /* BITSTREAM_H */\n' >> $@
	@echo "Wrote $@ ($$(wc -c < $<) bytes of bitstream)"

clean:
	rm -rf $(BUILD)

.PHONY: all sim wave synth header clean
