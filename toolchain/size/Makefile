# toolchain/size/Makefile — build our from-scratch section sizer (a HOST program, like GNU size).
# `make` -> build/size ; `make test` -> byte-identical to GNU size over objects, archives, executables.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/size: size.c $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/bfdsec.c $(COMMON)/bfdsec.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ size.c $(COMMON)/elfread.c $(COMMON)/arread.c $(COMMON)/bfdsec.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR size's output == GNU size's, byte for byte (stdout, stderr and the exit status), for every
# format/radix combination, over whatever real inputs the builds left behind.
test: $(BUILD)/size
	@GNU=$(abspath $(GNU)) SIZE=$(abspath $(BUILD)/size) bash tests/size-diff.sh

clean:
	rm -rf $(BUILD)
.PHONY: test clean
