# fpga-spi — top-level build (RTOS app)
#
# Two halves:
#   fpga/  — iCE40UP5K SPI-slave bitstream: latches a command byte from the
#            STM32 over the runtime SPI bus and drives two external LEDs.
#   src/   — STM32F411RE firmware, built against the RTOS (../../rtos): it
#            configures the FPGA at boot via the rtos iCE40 loader driver over
#            SPI3, then acts as SPI master on SPI1 to walk the two LEDs through
#            all states, reading them back (PC0/PC1) to self-verify the link.
#
# The bitstream is embedded into the firmware as a C array (build/bitstream.h,
# produced by the fpga/ sub-make), so it ships inside the STM32 flash image.
#
# Targets:
#   make        — build the bitstream + embed it + build the RTOS firmware
#   make sim    — run the FPGA SPI-slave testbench (no hardware)
#   make flash  — flash the firmware via ST-Link (openocd)
#   make clean

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size

OS      = ../../rtos
BUILD   = build

CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g \
          -I$(BUILD) -Isrc -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib -I$(OS)
LDFLAGS = -T src/linker.ld -nostdlib

-include $(BUILD)/config.mk

# ---- App + RTOS objects ----
OBJS := $(BUILD)/main.o

obj-y :=
include $(OS)/arch/arm/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))
obj-y :=
include $(OS)/kernel/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))
obj-y :=
include $(OS)/lib/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))
obj-y :=
include $(OS)/drivers/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h $(BUILD)/bitstream.h

# ---- Top target ----
all: $(BUILD)/fpga-spi.bin
	@$(SIZE) $(BUILD)/fpga-spi.elf

# 1. FPGA bitstream + C-array header (produced by the fpga/ sub-make into build/).
#    Depend on the FPGA sources so editing the RTL re-synthesizes the bitstream
#    (without these prereqs, an existing bitstream.h is treated as up-to-date and
#    RTL changes silently never reach the firmware).
$(BUILD)/bitstream.h: $(wildcard fpga/*.v) fpga/spi.pcf fpga/Makefile
	$(MAKE) -C fpga synth header

# 2. Generated config + devicetree
$(BUILD)/config.h $(BUILD)/config.mk: .config $(OS)/gen_config.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_config.py .config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts $(OS)/gen_devicetree.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_devicetree.py $< $@

# 3. Firmware
$(BUILD)/fpga-spi.elf: $(OBJS) src/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/fpga-spi.bin: $(BUILD)/fpga-spi.elf
	$(OBJCOPY) -O binary $< $@

# ---- Pattern rules ----
$(BUILD)/%.o: src/%.c $(GENERATED)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/arch/arm/%.s
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/arch/arm/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/kernel/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/lib/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/drivers/clock/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/drivers/serial/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/drivers/gpio/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/drivers/spi/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: $(OS)/drivers/fpga/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

sim:
	$(MAKE) -C fpga sim

flash: $(BUILD)/fpga-spi.elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD)/fpga-spi.elf verify reset exit"

clean:
	rm -rf build

.PHONY: all sim flash clean
