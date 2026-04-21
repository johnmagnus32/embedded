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

# ---- Load .config ----
-include $(BUILD)/config.mk

# ---- Collect objects from subdirectory Makefiles ----
# Each subdir Makefile sets obj-y and obj-$(CONFIG_X).
# We include them one at a time, collecting results.

OBJS := $(BUILD)/main.o

# arch/arm
obj-y :=
obj-n :=
include $(SRC)/arch/arm/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# kernel
obj-y :=
obj-n :=
include $(SRC)/kernel/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# lib
obj-y :=
obj-n :=
include $(SRC)/lib/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# fs
obj-y :=
obj-n :=
include $(SRC)/fs/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# drivers
obj-y :=
obj-n :=
include $(SRC)/drivers/Makefile
OBJS += $(addprefix $(BUILD)/,$(obj-y))

# ---- Generated headers ----
GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h

# ---- Targets ----

all: $(BUILD)/hello.bin
	@$(SIZE) $(BUILD)/hello.elf
	@echo ""
	@echo "Enabled: $(words $(OBJS)) objects"
	@echo "Flash with: st-flash write $(BUILD)/hello.bin 0x08000000"

$(BUILD)/config.h $(BUILD)/config.mk: .config gen_config.py
	python3 gen_config.py $< $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts gen_devicetree.py
	python3 gen_devicetree.py $< $@

$(BUILD)/hello.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/hello.bin: $(BUILD)/hello.elf
	$(OBJCOPY) -O binary $< $@

# ---- Pattern rules (one per source directory) ----

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

# ---- Inspection ----

disasm: $(BUILD)/hello.elf
	$(OBJDUMP) -d $<

sections: $(BUILD)/hello.elf
	$(SIZE) -A $<

symbols: $(BUILD)/hello.elf
	$(NM) -n $<

clean:
	rm -f $(BUILD)/*

.PHONY: all clean disasm sections symbols
