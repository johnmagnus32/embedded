# sample-stm32f411re/Makefile
#
# Build this sample against the OS:
#   cd sample-stm32f411re && make

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

OS      = ../os
BUILD   = build

CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -Og -g \
          -I$(BUILD) -Isrc -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib \
          -I$(OS)/fs -I$(OS)
LDFLAGS = -T linker.ld -nostdlib

# ---- Load .config ----
-include $(BUILD)/config.mk

# ---- Collect app objects (all src/*.c) ----
APP_SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(APP_SRCS))

# ---- Collect objects from OS ----

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

all: $(BUILD)/gameboy.bin
	@$(SIZE) $(BUILD)/gameboy.elf
	@echo ""
	@echo "Enabled: $(words $(OBJS)) objects"
	@echo "Flash: st-flash write $(BUILD)/gameboy.bin 0x08000000"

$(BUILD)/config.h $(BUILD)/config.mk: .config $(OS)/gen_config.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_config.py .config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts $(OS)/gen_devicetree.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_devicetree.py $< $@

$(BUILD)/gameboy.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/gameboy.bin: $(BUILD)/gameboy.elf
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

$(BUILD)/%.o: $(OS)/drivers/display/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/flash/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/ipc/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/drivers/ethernet/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Inspection ----

disasm: $(BUILD)/gameboy.elf
	$(OBJDUMP) -d $<

sections: $(BUILD)/gameboy.elf
	$(SIZE) -A $<

symbols: $(BUILD)/gameboy.elf
	$(NM) -n $<

defconfig:
	python3 $(OS)/gen_kconfig.py $(OS)/Kconfig
	mv .config .config 2>/dev/null; true

clean:
	rm -f $(BUILD)/*

.PHONY: all clean disasm sections symbols defconfig
