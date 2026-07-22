# Makefile — build the gameboy-v3 custom bootloader (Stage 1).
#
# Produces build/gv3boot.egon.bin: a BROM-bootable eGON image you dd to the SD
# at the 8 KiB offset (exactly where our U-Boot goes), replacing U-Boot.
#
# Uses the same pinned cross toolchain as the rest of the project (Step 0).
# Run ../scripts/00-toolchain.sh first, or `source ../scripts/env.sh`.

CROSS_COMPILE ?= arm-buildroot-linux-gnueabihf-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
HOSTCC  := cc

BUILD   := build
TARGET  := gv3boot

# Bare-metal, freestanding, ARMv7-A (Cortex-A7), no libc, no stdlib.
CFLAGS  := -mcpu=cortex-a7 -marm -ffreestanding -nostdlib -nostartfiles \
           -Os -Wall -Wextra -fno-builtin -fno-stack-protector
LDFLAGS := -T sram.ld -nostdlib -Wl,--build-id=none

OBJS := $(BUILD)/start.o $(BUILD)/main.o $(BUILD)/uart.o \
        $(BUILD)/dram_shim.o $(BUILD)/dram.o \
        $(BUILD)/sdcard.o $(BUILD)/fat.o $(BUILD)/fdt.o

.PHONY: all clean
all: $(BUILD)/$(TARGET).egon.bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Header deps listed explicitly so editing a shared header forces a rebuild
# (avoids the stale-object trap: a .h change must recompile its dependents).
HEADERS := uart.h dram.h dram_shim.h sdcard.h fat.h fdt.h

$(BUILD)/main.o: main.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: uart.c uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/dram_shim.o: dram_shim.c uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sdcard.o: sdcard.c sdcard.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fat.o: fat.c fat.h sdcard.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fdt.o: fdt.c fdt.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# dram.c is U-Boot's driver, copied verbatim — silence its warnings (unused
# members, sign compares) so our -Wall -Wextra stays meaningful for our code.
$(BUILD)/dram.o: dram.c dram.h dram_shim.h | $(BUILD)
	$(CC) $(CFLAGS) -w -c $< -o $@

# Link to an ELF at the SRAM load address.
$(BUILD)/$(TARGET).elf: $(OBJS) sram.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

# Flatten to a raw binary (this is what actually gets loaded to SRAM).
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Host tool that finalizes the eGON header (length + checksum).
$(BUILD)/mk-egon: mk-egon.c | $(BUILD)
	$(HOSTCC) -O2 -Wall -o $@ $<

# Finalize: pad to 512B, write length, compute BROM checksum.
$(BUILD)/$(TARGET).egon.bin: $(BUILD)/$(TARGET).bin $(BUILD)/mk-egon
	$(BUILD)/mk-egon $< $@
	@echo
	@echo "Built $@ — dd to the SD 8 KiB offset to boot it:"
	@echo "  dd if=$@ of=/dev/sdX bs=1024 seek=8 conv=fsync"

clean:
	rm -rf $(BUILD)
