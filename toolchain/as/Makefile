# toolchain/as/Makefile — build our from-scratch ARMv7 assembler (a HOST program, like GNU as).
# `make` -> build/as ; `make test` -> gold-compare vs GNU as + check GNU ld accepts our .o ; `make clean`.
# Built with the host cc (eventually self-hostable by our own cc).
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

# GNU binutils prefix used as the GOLD reference for `make test` (default: our stage-1 from-source
# toolchain). Override on the CLI, e.g. `make test GNU=arm-linux-gnueabihf-`. Tests SKIP if it's absent.
# GNU binutils 2.42 — the reference assembler/dumpers (the qemu build's cross toolchain)
GNU ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-

# Test inputs as `source.s:codesection` pairs — our .o's <codesection> bytes must match GNU as's exactly.
TESTS := tests/m1_dp.s:.text tests/m2_dp.s:.text tests/m2_ldst.s:.text tests/m2_data.s:.text tests/m2_wide.s:.text tests/m2_secsym.s:.rodata tests/m1_branch.s:.text tests/m2_branch.s:.text ../../libc/src/crt/crt0.S:.text._start \
         ../../libc/src/crt/crti.S:.init ../../libc/src/crt/crtn.S:.init

# Three layers, GNU-binutils-style: front-end (as.c) + machine-dependent backend (arm.c) + object
# backend (elf.c). Swapping arm.c for another arch backend is the ONLY change a new target needs.
OBJS := $(BUILD)/as.o $(BUILD)/arm.o $(BUILD)/elf.o $(BUILD)/elfutil.o

$(BUILD)/as: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c as.h $(COMMON)/elf.h $(COMMON)/elfutil.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/elfutil.o: $(COMMON)/elfutil.c $(COMMON)/elfutil.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# test: for each input, assemble with OUR as and GNU as, then compare the raw bytes of the code section
# (objcopy -O binary -j <sec>) — byte-identity proves our encoder matches GNU as. Finally, confirm GNU
# ld ACCEPTS our crt0 object (defsym stubs the external ref) — proves the ELF we emit is link-valid.
test: $(BUILD)/as
	@if [ ! -x "$(GNU)as" ]; then echo "SKIP: GNU as not found at '$(GNU)as' (set GNU=<prefix>)"; exit 0; fi; \
	pass=0; fail=0; t=$$(mktemp -d); \
	for spec in $(TESTS); do \
	  src=$${spec%%:*}; sec=$${spec##*:}; nm=$$(basename $$src); \
	  if ! $(BUILD)/as -o $$t/$$nm.ours.o $$src 2>$$t/err; then echo "FAIL $$nm: our as errored: $$(cat $$t/err)"; fail=$$((fail+1)); continue; fi; \
	  $(GNU)as -o $$t/$$nm.gnu.o $$src; \
	  $(GNU)objcopy -O binary -j $$sec $$t/$$nm.ours.o $$t/$$nm.ours.bin; \
	  $(GNU)objcopy -O binary -j $$sec $$t/$$nm.gnu.o  $$t/$$nm.gnu.bin; \
	  if cmp -s $$t/$$nm.ours.bin $$t/$$nm.gnu.bin; then \
	    echo "PASS $$nm — $$sec byte-identical to GNU as"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$nm — $$sec differs from GNU as:"; \
	    echo "  ours: $$(od -An -tx1 $$t/$$nm.ours.bin | tr -s ' ')"; \
	    echo "  gnu : $$(od -An -tx1 $$t/$$nm.gnu.bin  | tr -s ' ')"; fail=$$((fail+1)); \
	  fi; \
	done; \
	if $(GNU)ld -static -e _start --defsym __libc_start_main=0 -o $$t/crt0.elf $$t/crt0.S.ours.o 2>$$t/lderr; then \
	  echo "PASS ld-accept — GNU ld links our crt0.o into $$($(GNU)readelf -h $$t/crt0.elf | sed -n 's/.*Type:[[:space:]]*//p')"; pass=$$((pass+1)); \
	else echo "FAIL ld-accept — GNU ld rejected our crt0.o: $$(cat $$t/lderr)"; fail=$$((fail+1)); fi; \
	rm -rf $$t; \
	echo "----- $$pass passed, $$fail failed -----"; \
	[ $$fail -eq 0 ]

clean:
	rm -rf $(BUILD)

# The binutils 2.42 gas/arm testsuite (1010 dump + diagnostic tests), extracted from the cached source tarball
# and run through gastest/run.py (a port of binutils' run_dump_test/regexp_diff). GNU as runs through the same
# harness first; only tests IT passes are scored, per feature tier (core ARM, Thumb, FP/SIMD, M-profile, ...).
BINUTILS_TAR ?= ../../projects/gameboy-v3/image/build/downloads/binutils-2.42.tar.xz
GASTEST_WORK ?= /tmp/gastest
gas-suite: $(BUILD)/as
	@mkdir -p $(GASTEST_WORK); [ -d $(GASTEST_WORK)/binutils-2.42/gas/testsuite/gas/arm ] || \
	  tar -xJf $(BINUTILS_TAR) -C $(GASTEST_WORK) binutils-2.42/gas/testsuite/gas/arm
	@export GNU=$(abspath $(dir $(GNU)))/$(notdir $(GNU)) GASTEST_WORK=$(GASTEST_WORK); \
	  python3 gastest/run.py --as gnu >/dev/null && python3 gastest/run.py --as ours >/dev/null && python3 gastest/score.py

.PHONY: clean test gas-suite
