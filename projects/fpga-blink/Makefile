# fpga-blink Makefile
# Targets: sim (Icarus), synth (Yosys+nextpnr), prog (iceprog)

PROJ = blink
PCF  = icebreaker.pcf

# Simulation (Icarus Verilog)
sim: tb_$(PROJ).v
	iverilog -o sim_$(PROJ) tb_$(PROJ).v
	vvp sim_$(PROJ)

wave: sim
	gtkwave $(PROJ).vcd

# Synthesis (Yosys + nextpnr)
synth: $(PROJ).bin

$(PROJ).json: $(PROJ).v
	yosys -p "synth_ice40 -top $(PROJ) -json $@" $<

$(PROJ).asc: $(PROJ).json $(PCF)
	nextpnr-ice40 --up5k --package sg48 --json $< --pcf $(PCF) --asc $@

$(PROJ).bin: $(PROJ).asc
	icepack $< $@

# Program the iCEBreaker
prog: $(PROJ).bin
	iceprog $<

# Clean
clean:
	rm -f $(PROJ).json $(PROJ).asc $(PROJ).bin $(PROJ).vcd sim_$(PROJ)

.PHONY: sim wave synth prog clean
