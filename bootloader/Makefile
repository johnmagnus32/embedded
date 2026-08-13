# Makefile — build the gameboy-v3 custom bootloader (Stage 1).
#
# Produces build/gv3boot.egon.bin: a BROM-bootable eGON image you dd to the SD
# at the 8 KiB offset (exactly where our U-Boot goes), replacing U-Boot.
#
# Uses the same pinned cross toolchain as the rest of the project (Step 0).
# Run forge/core/backends/toolchain.sh (or `make toolchain`) first, or `source forge/core/backends/lib.sh`.

CROSS_COMPILE ?= arm-buildroot-linux-gnueabihf-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
HOSTCC  := cc

BUILD   := build
TARGET  := gv3boot

# Bare-metal, freestanding, ARMv7-A (Cortex-A7), no libc, no stdlib.
CFLAGS  := -mcpu=cortex-a7 -marm -ffreestanding -nostdlib -nostartfiles \
           -Os -Wall -Wextra -fno-builtin -fno-stack-protector

# Link/run address. 0x20000 = the SPL slot the BROM loads a MEDIUM boot into.
# For FEL delivery, xfel can't bulk-write 0x20000 (collides with the live FEL USB
# buffers in SRAM A1), so build the FEL image with LOAD_ADDR=0x28000:
#   make LOAD_ADDR=0x28000       # FEL-loadable image (xfel write 0x28000 + exec)
#   make                          # default 0x20000 (for eventual BROM medium boot)
LOAD_ADDR ?= 0x00020000
LDFLAGS := -T sram.ld -nostdlib -Wl,--build-id=none -Wl,--defsym=LOAD_ADDR=$(LOAD_ADDR)

OBJS := $(BUILD)/start.o $(BUILD)/main.o $(BUILD)/uart.o \
        $(BUILD)/dram_shim.o $(BUILD)/dram.o \
        $(BUILD)/sdcard.o $(BUILD)/fat.o $(BUILD)/fdt.o $(BUILD)/spinor.o

.PHONY: all clean fel
all: $(BUILD)/$(TARGET).egon.bin

# FEL-loadable image: linked at 0x28000 (xfel can't bulk-write the 0x20000 SPL
# slot — it collides with the live FEL USB buffers in SRAM A1). Named distinctly
# so tools/flash.sh (BOOTLOADER=custom) can FEL-load + exec it. Build with:
#   make fel        (equivalent to: make LOAD_ADDR=0x28000 + rename)
fel:
	$(MAKE) clean
	$(MAKE) LOAD_ADDR=0x28000
	cp $(BUILD)/$(TARGET).egon.bin $(BUILD)/$(TARGET)-fel-0x28000.egon.bin
	@echo "Built $(BUILD)/$(TARGET)-fel-0x28000.egon.bin — FEL-load: xfel write 0x28000 <it>; xfel exec 0x28000"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Header deps listed explicitly so editing a shared header forces a rebuild
# (avoids the stale-object trap: a .h change must recompile its dependents).
HEADERS := uart.h dram.h dram_shim.h sdcard.h fat.h fdt.h spinor.h nor_layout.h

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

# fdt.c is built at -O0 on purpose. At -Os, GCC 10.3 (this toolchain) miscompiles
# the FDT structure-block walk in fdt_set_bootargs so it hangs on real silicon
# (the exact same bytes parse fine on the host and at -O0). No warnings at
# -Wall -Wextra -Wmaybe-uninitialized -Wstrict-aliasing, and no missing memcpy/
# memset — so it's an optimizer/UB heisenbug not worth chasing in a run-once FDT
# walker. -O0 is correct; the file is tiny, so the size/speed cost is irrelevant.
$(BUILD)/fdt.o: fdt.c fdt.h | $(BUILD)
	$(CC) $(CFLAGS) -O0 -c $< -o $@

$(BUILD)/spinor.o: spinor.c spinor.h | $(BUILD)
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
