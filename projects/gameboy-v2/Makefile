# gameboy-v2/Makefile
#
# Unified build: FPGA bitstream + tile assets + firmware → one .bin
# Usage: make

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size

OS      = ../../rtos
BUILD   = build

CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g \
          -I$(BUILD) -Isrc -Iassets -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib \
          -I$(OS)/fs -I$(OS)
LDFLAGS = -T linker.ld -nostdlib

# ---- Load .config ----
-include $(BUILD)/config.mk

# ---- App sources ----
APP_SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(APP_SRCS))

# ---- Generated assets (raw binaries embedded via objcopy) ----
OBJS += $(BUILD)/fpga_bitstream.o $(BUILD)/tiles_data.o $(BUILD)/tile_table.o

# ---- OS objects ----
obj-y :=
obj-n :=
include $(OS)/arch/arm/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

obj-y :=
obj-n :=
include $(OS)/kernel/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

obj-y :=
obj-n :=
include $(OS)/lib/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

obj-y :=
obj-n :=
include $(OS)/fs/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

obj-y :=
obj-n :=
include $(OS)/drivers/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# ---- Generated headers ----
GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h

# ---- Targets ----

all: $(BUILD)/gameboy-v2.bin $(BUILD)/ppu_top.json test
	@$(SIZE) $(BUILD)/gameboy-v2.elf
	@echo ""
	@echo "Flash: st-flash write $(BUILD)/gameboy-v2.bin 0x08000000"

# ---- FPGA bitstream → linkable object ----

FPGA_SRCS = $(wildcard fpga/*.v)
FPGA_PCF  = fpga/ice40-breakout.pcf

$(BUILD)/ppu_top.json: $(FPGA_SRCS)
	@mkdir -p $(BUILD)
	yosys -p "read_verilog $(FPGA_SRCS); synth_ice40 -top ppu_top -json $@"

$(BUILD)/ppu_top.asc: $(BUILD)/ppu_top.json $(FPGA_PCF)
	nextpnr-ice40 --up5k --package sg48 --freq 20 --json $< --pcf $(FPGA_PCF) --asc $@

$(BUILD)/ppu.bin: $(BUILD)/ppu_top.asc
	icepack $< $@

$(BUILD)/fpga_bitstream.o: $(BUILD)/ppu.bin
	$(OBJCOPY) -I binary -O elf32-littlearm \
		--rename-section .data=.fpga_bitstream,alloc,load,readonly,contents \
		$< $@

# Build just the FPGA bitstream
fpga: $(BUILD)/ppu.bin
	@echo "Bitstream: $< ($$(stat -c%s $<) bytes)"

# ---- Tile assets → linkable objects ----

assets/tiles.bin assets/tile_table.bin: assets/convert_tiles.py
	cd assets && python3 convert_tiles.py

$(BUILD)/tiles_data.o: assets/tiles.bin
	$(OBJCOPY) -I binary -O elf32-littlearm \
		--rename-section .data=.tiles_data,alloc,load,readonly,contents \
		$< $@

$(BUILD)/tile_table.o: assets/tile_table.bin
	$(OBJCOPY) -I binary -O elf32-littlearm \
		--rename-section .data=.tile_table,alloc,load,readonly,contents \
		$< $@

# ---- Firmware link ----

$(BUILD)/config.h $(BUILD)/config.mk: .config $(OS)/gen_config.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_config.py .config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts $(OS)/gen_devicetree.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_devicetree.py $< $@

$(BUILD)/gameboy-v2.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/gameboy-v2.bin: $(BUILD)/gameboy-v2.elf
	$(OBJCOPY) -O binary $< $@

# ---- Pattern rules ----

$(BUILD)/%.o: src/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/arch/arm/%.s
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/arch/arm/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/kernel/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/lib/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/fs/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/gpio/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/spi/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/serial/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/clock/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/dma/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/adc/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/i2s/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/flash/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/fpga/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Audio ----

audio: $(BUILD)/songs.bin

$(BUILD)/songs.bin: assets/music.wav | $(BUILD)
	ffmpeg -y -i $< -ar 22050 -ac 1 -f u8 $(BUILD)/raw.pcm 2>/dev/null
	python3 -c "import struct; d=open('$(BUILD)/raw.pcm','rb').read(); \
		f=open('$@','wb'); f.write(struct.pack('<I',len(d))+d); \
		print(f'songs.bin: {len(d)} bytes ({len(d)/22050:.1f}s)')"
	rm -f $(BUILD)/raw.pcm

clean:
	rm -rf $(BUILD)

# ---- FPGA testbenches (Icarus Verilog) ----

IVERILOG = iverilog
VVP = vvp
TB_DIR = tests/fpga
TB_TESTS = tb_tile_table tb_sprite_table tb_spi_cmd tb_pixel_gen tb_lcd_driver tb_ppu_e2e

test: $(TB_TESTS:%=$(BUILD)/test/%.result)
	@echo "" && grep -l FAIL $(BUILD)/test/*.result 2>/dev/null && exit 1 || echo "All FPGA tests passed"

$(BUILD)/test/tb_tile_table.result: $(TB_DIR)/tb_tile_table.v fpga/tile_table.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	$(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

$(BUILD)/test/tb_sprite_table.result: $(TB_DIR)/tb_sprite_table.v fpga/sprite_table.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	$(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

$(BUILD)/test/tb_spi_cmd.result: $(TB_DIR)/tb_spi_cmd.v fpga/spi_cmd.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	$(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

$(BUILD)/test/tb_pixel_gen.result: $(TB_DIR)/tb_pixel_gen.v fpga/pixel_gen.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	$(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

$(BUILD)/test/tb_lcd_driver.result: $(TB_DIR)/tb_lcd_driver.v fpga/lcd_driver.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	$(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

$(BUILD)/test/tb_ppu_e2e.result: $(TB_DIR)/tb_ppu_e2e.v $(TB_DIR)/sb_spram_sim.v fpga/spi_cmd.v fpga/sprite_table.v fpga/tile_table.v fpga/pixel_gen.v fpga/lcd_driver.v fpga/sprite_mem.v
	@mkdir -p $(BUILD)/test
	$(IVERILOG) -o $(BUILD)/test/$(@F:.result=.vvp) $^
	timeout 60 $(VVP) $(BUILD)/test/$(@F:.result=.vvp) > $@ 2>&1
	@cat $@

# ---- Gate-level e2e test (Icarus + cells_sim.v) ----
CELLS_SIM = $(shell yosys-config --datdir)/ice40/cells_sim.v

test-gate: $(BUILD)/ppu_top.json
	@echo "=== Gate-level e2e test (Icarus) ==="
	yosys -q -p "read_verilog $(FPGA_SRCS); read_verilog tests/fpga/ppu_test_top.v; synth_ice40 -top ppu_test_top; write_verilog $(BUILD)/test/ppu_gate.v"
	$(IVERILOG) -g2012 -o $(BUILD)/test/ppu_gate.vvp $(CELLS_SIM) $(BUILD)/test/ppu_gate.v tests/fpga/tb_ppu_gate_level.v
	timeout 120 $(VVP) $(BUILD)/test/ppu_gate.vvp | tee $(BUILD)/test/ppu_gate.result
	@grep -q "PASS" $(BUILD)/test/ppu_gate.result

.PHONY: all clean audio fpga test test-gate
