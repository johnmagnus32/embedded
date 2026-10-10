# toolchain/readelf/Makefile — build our from-scratch ELF reporter (a HOST program, like GNU readelf).
# `make` -> build/readelf ; `make test` -> byte-identical to GNU readelf for -h/-S/-s/-r/-d/-n over real inputs.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/readelf: readelf.c arm-reloc-names.h $(COMMON)/elfver.c $(COMMON)/elfver.h $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ readelf.c $(COMMON)/elfread.c $(COMMON)/arread.c $(COMMON)/elfver.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR readelf's output == GNU readelf's, byte for byte, for every option set below, over objects, archives,
# executables and shared objects (tests/readelf-diff.sh).
test: $(BUILD)/readelf
	@GNU=$(abspath $(GNU)) READELF=$(abspath $(BUILD)/readelf) bash tests/readelf-diff.sh

clean:
	rm -rf $(BUILD)

.PHONY: clean test
