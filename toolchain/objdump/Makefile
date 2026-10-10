# toolchain/objdump/Makefile — build our from-scratch ELF structure dumper (a HOST program, like GNU objdump).
# `make` -> build/objdump ; `make test` -> byte-identical to GNU objdump for -h/-t/-T/-r/-R over real inputs.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/objdump: objdump.c arm-reloc-names.h $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ objdump.c $(COMMON)/elfread.c $(COMMON)/arread.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR objdump's output == GNU objdump's, byte for byte, for every option set below, over objects, archives,
# executables and shared objects (tests/objdump-diff.sh).
test: $(BUILD)/objdump
	@GNU=$(abspath $(GNU)) OBJDUMP=$(abspath $(BUILD)/objdump) bash tests/objdump-diff.sh

clean:
	rm -rf $(BUILD)

.PHONY: clean test
