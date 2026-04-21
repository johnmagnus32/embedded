TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

# Directories
SRC     = src
BUILD   = build

# Flags
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -O2 -I$(BUILD) -I$(SRC) -save-temps=obj
LDFLAGS = -T linker.ld -nostdlib

# Source files → object files in build/
OBJS    = $(BUILD)/startup.o $(BUILD)/main.o $(BUILD)/uart.o $(BUILD)/gpio.o \
          $(BUILD)/gpio_keys.o $(BUILD)/gpio_leds.o $(BUILD)/sched.o $(BUILD)/log.o \
          $(BUILD)/systick.o

# Default target
all: $(BUILD)/hello.bin
	@$(SIZE) $(BUILD)/hello.elf
	@echo ""
	@echo "Flash with: st-flash write $(BUILD)/hello.bin 0x08000000"

# Generate devicetree.h from board.dts (like Zephyr's gen_defines.py)
$(BUILD)/devicetree.h: board.dts gen_devicetree.py
	python3 gen_devicetree.py $< $@

# Link all objects into ELF
$(BUILD)/hello.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Raw binary for flashing
$(BUILD)/hello.bin: $(BUILD)/hello.elf
	$(OBJCOPY) -O binary $< $@

# Compile C → object (depends on generated devicetree.h)
$(BUILD)/%.o: $(SRC)/%.c $(BUILD)/devicetree.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble → object
$(BUILD)/%.o: $(SRC)/%.s
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile drivers
$(BUILD)/%.o: $(SRC)/drivers/%.c $(BUILD)/devicetree.h
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
