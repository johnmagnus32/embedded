# os/tests/Makefile — Build and run OS tests on QEMU lm3s6965evb

TC      = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC      = $(TC)-gcc
OBJCOPY = $(TC)-objcopy
SIZE    = $(TC)-size

OS      = ..
BUILD   = build
QEMU    = qemu-system-arm

CFLAGS  = -mcpu=cortex-m3 -mthumb -nostdlib -ffreestanding -Wall -Og -g \
          -I$(BUILD) -Isrc -I$(OS)/include -I$(OS)/kernel -I$(OS)/lib -I$(OS)
LDFLAGS = -T linker.ld -nostdlib

# ---- Load .config ----
-include $(BUILD)/config.mk

# ---- Test sources ----
APP_SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(APP_SRCS))

# ---- OS objects (kernel + arch only, no drivers) ----
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

# ---- Generated headers ----
GENERATED = $(BUILD)/config.h $(BUILD)/devicetree.h

# ---- Targets ----

all: $(BUILD)/test.elf
	@$(SIZE) $<

$(BUILD)/config.h $(BUILD)/config.mk: .config $(OS)/gen_config.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_config.py .config $(BUILD)/config.h $(BUILD)/config.mk

$(BUILD)/devicetree.h: board.dts $(OS)/gen_devicetree.py
	@mkdir -p $(BUILD)
	python3 $(OS)/gen_devicetree.py $< $@

$(BUILD)/test.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: src/%.c $(GENERATED)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/arch/arm/%.s
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/arch/arm/%.c $(GENERATED)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/kernel/%.c $(GENERATED)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(OS)/lib/%.c $(GENERATED)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Run on QEMU ----
test: all
	@echo "Running on QEMU..."
	@timeout 5 $(QEMU) -machine lm3s6965evb -nographic \
		-kernel $(BUILD)/test.elf 2>/dev/null || true
	@echo ""

clean:
	rm -f $(BUILD)/*

.PHONY: all test clean
