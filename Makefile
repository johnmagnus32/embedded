TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

# Directories
SRC     = src
BUILD   = build

# Include paths: generated headers, public API, all subsystems
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -O2 \
          -I$(BUILD) -I$(SRC)/include -I$(SRC)/kernel -I$(SRC)/lib \
          -I$(SRC)/drivers -I$(SRC)
LDFLAGS = -T linker.ld -nostdlib

# Source files organized by subsystem
OBJS    = $(BUILD)/startup.o $(BUILD)/systick.o \
          $(BUILD)/sched.o $(BUILD)/sync.o $(BUILD)/msgq.o \
          $(BUILD)/heap.o $(BUILD)/memslab.o $(BUILD)/log.o \
          $(BUILD)/clock.o $(BUILD)/uart.o $(BUILD)/gpio.o \
          $(BUILD)/gpio_keys.o $(BUILD)/gpio_leds.o \
          $(BUILD)/spi.o $(BUILD)/flash.o \
          $(BUILD)/main.o

# Default target
all: $(BUILD)/hello.bin
	@$(SIZE) $(BUILD)/hello.elf
	@echo ""
	@echo "Flash with: st-flash write $(BUILD)/hello.bin 0x08000000"

# Generate devicetree.h from board.dts
$(BUILD)/devicetree.h: board.dts gen_devicetree.py
	python3 gen_devicetree.py $< $@

# Link
$(BUILD)/hello.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Raw binary
$(BUILD)/hello.bin: $(BUILD)/hello.elf
	$(OBJCOPY) -O binary $< $@

# Pattern rules for each source directory
$(BUILD)/%.o: $(SRC)/arch/arm/%.s
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/arch/arm/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/kernel/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/lib/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/drivers/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Inspection
disasm: $(BUILD)/hello.elf
	$(OBJDUMP) -d $<

sections: $(BUILD)/hello.elf
	$(SIZE) -A $<

symbols: $(BUILD)/hello.elf
	$(NM) -n $<

clean:
	rm -f $(BUILD)/*

.PHONY: all clean disasm sections symbols
