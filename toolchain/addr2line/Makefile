# toolchain/addr2line/Makefile — build our from-scratch addr2line (a HOST program, like GNU addr2line).
# `make` -> build/addr2line ; `make test` -> output identical to GNU addr2line over real inputs.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/addr2line: addr2line.c $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ addr2line.c $(COMMON)/elfread.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR addr2line's answers == GNU addr2line's, over objects and executables built by BOTH toolchains,
# for every row of their line tables and for every option combination.
test: $(BUILD)/addr2line
	@GNU=$(abspath $(GNU)) A2L=$(abspath $(BUILD)/addr2line) TC=$(abspath ..) bash tests/a2l-diff.sh

clean:
	rm -rf $(BUILD)
.PHONY: test clean
