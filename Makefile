# Top-level Makefile
#
# Builds a sample application against the OS, like:
#   make SAMPLE=sample-stm32f411re
#
# The OS is in os/ (like the Zephyr repo).
# The sample provides: main.c, board.dts, .config, linker.ld

SAMPLE ?= sample-stm32f411re

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size
OBJDUMP = $(TC)-objdump
NM      = $(TC)-nm

OS      = os
APP     = $(SAMPLE)
BUILD   = build

# Include paths: generated headers, OS headers, all subsystems
CFLAGS  = -mcpu=cortex-m4 -mthumb -nostdlib -ffreestanding -Wall -O2 \
          -I$(BUILD) -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib \
          -I$(OS)/fs -I$(OS)/drivers -I$(OS)
LDFLAGS = -T $(APP)/linker.ld -nostdlib

# ---- Load .config from the sample ----
-include $(BUILD)/config.mk

# ---- Collect objects from OS subdirectory Makefiles ----

OBJS := $(BUILD)/main.o

# OS subsystems
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

all: $(BUILD)/hello.bin
	@$(SIZE) $(BUILD)/hello.elf
	@echo ""
	@echo "Sample: $(SAMPLE)"
	@echo "Enabled: $(words $(OBJS)) objects"
	@echo "Flash with: st-flash write $(BUILD)/hello.bin 0x08000000"

$(BUILD)/config.h $(BUILD)/config.mk: $(APP)/.config $(OS)/gen_config.py
	python3 $(OS)/gen_config.py $(APP)/.config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: $(APP)/board.dts $(OS)/gen_devicetree.py
	python3 $(OS)/gen_devicetree.py $< $@

$(BUILD)/hello.elf: $(OBJS) $(APP)/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/hello.bin: $(BUILD)/hello.elf
	$(OBJCOPY) -O binary $< $@

# ---- Pattern rules ----

# Application source
$(BUILD)/%.o: $(APP)/src/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

# OS sources
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

$(BUILD)/%.o: $(OS)/drivers/%.c $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Inspection ----

disasm: $(BUILD)/hello.elf
	$(OBJDUMP) -d $<

sections: $(BUILD)/hello.elf
	$(SIZE) -A $<

symbols: $(BUILD)/hello.elf
	$(NM) -n $<

# Generate .config from Kconfig
menuconfig:
	python3 $(OS)/gen_kconfig.py $(OS)/Kconfig --interactive
	cp .config $(APP)/.config

defconfig:
	python3 $(OS)/gen_kconfig.py $(OS)/Kconfig
	cp .config $(APP)/.config

clean:
	rm -f $(BUILD)/*

.PHONY: all clean disasm sections symbols menuconfig defconfig
