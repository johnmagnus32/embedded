# fpga-blink / FPGA Makefile
#
#   make sim    — run the testbench (no hardware), prints PASS/FAIL
#   make synth  — synthesize + place/route -> ../build/blink.bin (the bitstream)
#   make header — convert ../build/blink.bin -> ../build/bitstream.h for the STM32
#   make        — sim + synth + header
#
# All artifacts go to the shared project build dir (../build); this directory
# keeps only source. Target board: ice40-breakout (iCE40UP5K-SG48), configured
# over SPI by an STM32 at boot. No `make prog` — the bitstream is embedded into
# the STM32 firmware (see ../src) rather than flashed directly to the FPGA.

PROJ  = blink
TOP   = blink_top          # synthesis top (hard IP wrapper)
LOGIC = blink.v            # pure logic (also used by the testbench)
WRAP  = blink_top.v        # hard IP wrapper (SB_HFOSC)
PCF   = blink.pcf
TB    = tb_blink.v
# shared project build dir (no inline comment — trailing spaces would corrupt it)
BUILD = ../build

all: sim synth header

# --- Simulation (Icarus Verilog): tests pure logic, no hard IP ---
sim: $(TB) $(LOGIC)
	@mkdir -p $(BUILD)
	iverilog -o $(BUILD)/sim_$(PROJ) $(TB) $(LOGIC)
	cd $(BUILD) && vvp sim_$(PROJ)   # writes blink.vcd here, keeping fpga/ clean

wave: sim
	gtkwave $(BUILD)/$(PROJ).vcd

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
