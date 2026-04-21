TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

SRC     = src
BUILD   = build

CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -O2 \
          -I$(BUILD) -I$(SRC)/include -I$(SRC)/kernel -I$(SRC)/lib \
          -I$(SRC)/fs -I$(SRC)/drivers -I$(SRC)
LDFLAGS = -T linker.ld -nostdlib

# Include generated config (CONFIG_X := y|n)
-include $(BUILD)/config.mk

# ---- Conditional object list (like Zephyr's CMakeLists + Kconfig) ----

# Always included
OBJS    = $(BUILD)/startup.o $(BUILD)/main.o

# Drivers
OBJS-$(CONFIG_CLOCK)      += $(BUILD)/clock.o
OBJS-$(CONFIG_UART)       += $(BUILD)/uart.o
OBJS-$(CONFIG_GPIO)       += $(BUILD)/gpio.o
OBJS-$(CONFIG_GPIO_KEYS)  += $(BUILD)/gpio_keys.o
OBJS-$(CONFIG_GPIO_LEDS)  += $(BUILD)/gpio_leds.o
OBJS-$(CONFIG_SPI)        += $(BUILD)/spi.o
OBJS-$(CONFIG_FLASH)      += $(BUILD)/flash.o

# Kernel
OBJS-$(CONFIG_SCHED)      += $(BUILD)/sched.o
OBJS-$(CONFIG_SYSTICK)    += $(BUILD)/systick.o
OBJS-$(CONFIG_SYNC)       += $(BUILD)/sync.o
OBJS-$(CONFIG_MSGQ)       += $(BUILD)/msgq.o

# Libraries
OBJS-$(CONFIG_HEAP)       += $(BUILD)/heap.o
OBJS-$(CONFIG_MEMSLAB)    += $(BUILD)/memslab.o
OBJS-$(CONFIG_LOG)        += $(BUILD)/log.o

# Filesystem
OBJS-$(CONFIG_FS)         += $(BUILD)/fs.o
OBJS-$(CONFIG_TINYFS)     += $(BUILD)/tinyfs.o

# Collect all enabled objects
OBJS += $(OBJS-y)

# Default target
all: $(BUILD)/hello.bin
	@$(SIZE) $(BUILD)/hello.elf
	@echo ""
	@echo "Enabled: $(words $(OBJS)) objects from .config"
	@echo "Flash with: st-flash write $(BUILD)/hello.bin 0x08000000"

# Generate config.h and config.mk from .config
$(BUILD)/config.h $(BUILD)/config.mk: .config gen_config.py
	python3 gen_config.py $< $(BUILD)/config.h $(BUILD)/config.mk

# Generate devicetree.h from board.dts
$(BUILD)/devicetree.h: board.dts gen_devicetree.py
	python3 gen_devicetree.py $< $@

# All C objects depend on both generated headers
GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h

# Link
$(BUILD)/hello.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/hello.bin: $(BUILD)/hello.elf
	$(OBJCOPY) -O binary $< $@

# Pattern rules for each source directory
$(BUILD)/%.o: $(SRC)/arch/arm/%.s
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/arch/arm/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/kernel/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/lib/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/fs/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/drivers/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c $(GENERATED)
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
