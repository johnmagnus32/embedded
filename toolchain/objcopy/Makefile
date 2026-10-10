# toolchain/objcopy/Makefile — build our from-scratch objcopy (+ strip: the same program, run by that name).
# `make` -> build/objcopy, build/strip ; `make test` -> byte-identical to GNU objcopy/strip ; `make clean`.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

all: $(BUILD)/objcopy $(BUILD)/strip

$(BUILD)/objcopy: objcopy.c $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/strmap.c $(COMMON)/strmap.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ objcopy.c $(COMMON)/elfread.c $(COMMON)/arread.c $(COMMON)/strmap.c

$(BUILD)/strip: $(BUILD)/objcopy
	ln -sf objcopy $@

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR objcopy/strip output == GNU's, byte for byte (and the same file modes), for the option sets the kernel,
# BusyBox and U-Boot builds use, over the inputs the image build left behind.
test: all
	@for t in cpp cc as ld rt; do $(MAKE) -s -C ../$$t >/dev/null || exit 1; done
	@GNU=$(abspath $(GNU)) OBJCOPY=$(abspath $(BUILD)/objcopy) STRIP=$(abspath $(BUILD)/strip) bash tests/objcopy-diff.sh

clean:
	rm -rf $(BUILD)

.PHONY: all clean test
