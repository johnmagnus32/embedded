# toolchain/nm/Makefile — build our from-scratch symbol lister (a HOST program, like GNU nm).
# `make` -> build/nm ; `make test` -> byte-identical to GNU nm over objects, archives, executables, shared objects.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/nm: nm.c $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ nm.c $(COMMON)/elfread.c $(COMMON)/arread.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR nm's output == GNU nm's, byte for byte (stdout and stderr), for every option set below, over the
# symbol-class probe (tests/classes.s, assembled by GNU as) and whatever real inputs the image build left behind.
test: $(BUILD)/nm
	@GNU=$(GNU) NM=$(BUILD)/nm bash tests/nm-diff.sh

clean:
	rm -rf $(BUILD)

.PHONY: clean test
