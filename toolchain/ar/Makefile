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
	@# ranlib — the same program under another name (argv[0]), indexing an archive made without one
	@t=$$(mktemp -d); ln -sf $(abspath $(BUILD)/ar) $$t/ranlib; 	printf '.global r1\nr1: nop\n' | $(GNU)as -o $$t/r.o; printf '.global r2\nr2: bx lr\n' | $(GNU)as -o $$t/s.o; 	for who in g o; do $(GNU)ar rcSD $$t/$$who.a $$t/r.o $$t/s.o; done; 	$(GNU)ranlib -D $$t/g.a; $$t/ranlib -D $$t/o.a; 	if cmp -s $$t/g.a $$t/o.a; then echo "PASS ranlib -D: the index is byte-identical to GNU ranlib's"; else 	  echo "FAIL ranlib: $$(cmp $$t/g.a $$t/o.a 2>&1)"; rm -rf $$t; exit 1; fi; 	$$t/ranlib $$t/nosuch.a 2>$$t/e; [ $$? = 1 ] && grep -q 'ranlib: ' $$t/e 	  && echo "PASS ranlib names itself when it fails: $$(cat $$t/e)" 	  || { echo "FAIL ranlib on a missing archive: '$$(cat $$t/e)'"; rm -rf $$t; exit 1; }; rm -rf $$t

clean:
	rm -rf $(BUILD)

.PHONY: clean test
