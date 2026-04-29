# sample-rp2040/Makefile
#
# Build: cd sample-rp2040 && make

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

OS      = ../os
BUILD   = build

# Cortex-M0+ flags (different from M4!)
CFLAGS  = -mcpu=cortex-m0plus -mthumb -nostdlib -ffreestanding -Wall -O2 \
          -I$(BUILD) -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib \
          -I$(OS)/fs -I$(OS)
LDFLAGS = -T linker.ld -nostdlib -lgcc

-include $(BUILD)/config.mk

# ---- Collect objects ----

OBJS := $(BUILD)/main.o

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

GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h

all: $(BUILD)/firmware.bin
	@$(SIZE) $(BUILD)/firmware.elf
	@echo ""
	@echo "Enabled: $(words $(OBJS)) objects"

$(BUILD)/config.h $(BUILD)/config.mk: .config $(OS)/gen_config.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_config.py .config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts $(OS)/gen_devicetree.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_devicetree.py $< $@

$(BUILD)/firmware.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) -T linker.ld -nostdlib -o $@ $(OBJS) -lgcc

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary $< $@

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

$(BUILD)/%.o: $(OS)/drivers/display/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/flash/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/ipc/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/ethernet/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

symbols: $(BUILD)/firmware.elf
	$(NM) -n $<

clean:
	rm -f $(BUILD)/*

.PHONY: all clean symbols
