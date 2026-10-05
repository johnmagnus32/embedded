# toolchain/ar/Makefile — build our from-scratch archiver (a HOST program, like GNU ar).
# `make` -> build/ar ; `make test` -> byte-identical to GNU ar (tests/ar-diff.sh) ; `make clean`.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

$(BUILD)/ar: ar.c $(COMMON)/elfread.c $(COMMON)/elfread.h $(COMMON)/arread.c $(COMMON)/arread.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ ar.c $(COMMON)/elfread.c $(COMMON)/arread.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: OUR ar == GNU ar, byte for byte (archives, listings, extracted files, exit status) — tests/ar-diff.sh —
# plus an archive our ld links from, and a truncated object refused.
test: $(BUILD)/ar
	@if [ ! -x "$(GNU)ar" ]; then echo "SKIP: GNU ar not found at '$(GNU)ar' (set GNU=<prefix>)"; exit 0; fi
	@GNU=$(abspath $(GNU)) AR=$(abspath $(BUILD)/ar) AR_SETUP=$(abspath $(BUILD)/ar) bash tests/ar-diff.sh
	@t=$$(mktemp -d); printf '.global a_one\na_one: mov r0, #1\nbx lr\n' | $(GNU)as -o $$t/a.o; \
	head -c 200 $$t/a.o > $$t/cut.o; $(BUILD)/ar rc $$t/bad.a $$t/cut.o 2>$$t/err; st=$$?; \
	[ $$st = 1 ] && grep -q 'outside the file' $$t/err && echo "PASS truncated object rejected: $$(cat $$t/err)" \
	  || { echo "FAIL truncated object: exit $$st, '$$(cat $$t/err)'"; rm -rf $$t; exit 1; }; rm -rf $$t

clean:
	rm -rf $(BUILD)

.PHONY: clean test
