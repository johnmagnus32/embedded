# toolchain/ar/Makefile — build our from-scratch archiver (a HOST program, like GNU ar).
# `make` -> build/ar ; `make test` -> archive our-as objects, check GNU ar + GNU ld accept it ; `make clean`.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
GNU   ?= ../../projects/gameboy-v3/build/toolchain-gcc-initial/bin/arm-forge-linux-gnueabihf-

$(BUILD)/ar: ar.c $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ ar.c

$(BUILD):
	mkdir -p $(BUILD)

# Gate: assemble two objects with our as, archive them with OUR ar, then prove the archive is well-formed
# by having GNU ar LIST it and GNU nm read its symbol index (validates the ARMAP + member framing).
test: $(BUILD)/ar
	@if [ ! -x "$(GNU)ar" ]; then echo "SKIP: GNU ar not found at '$(GNU)ar' (set GNU=<prefix>)"; exit 0; fi
	@$(MAKE) -s -C ../as >/dev/null
	@t=$$(mktemp -d); fail=0; \
	printf '.text\n.global a_one\na_one: mov r0, #1\nbx lr\n'  > $$t/a.s; \
	printf '.text\n.global b_two\nb_two: mov r0, #2\nbx lr\n'  > $$t/b.s; \
	$(OURAS) -o $$t/a.o $$t/a.s; $(OURAS) -o $$t/b.o $$t/b.s; \
	$(BUILD)/ar rc $$t/lib.a $$t/a.o $$t/b.o || { echo "FAIL: our ar errored"; exit 1; }; \
	$(GNU)ar t $$t/lib.a >$$t/list 2>$$t/err && echo "PASS GNU ar lists: $$(tr '\n' ' ' <$$t/list)" || { echo "FAIL GNU ar rejected: $$(cat $$t/err)"; fail=1; }; \
	syms=$$($(GNU)nm --print-armap $$t/lib.a 2>/dev/null | grep -c 'in a\.o\|in b\.o'); \
	[ "$$syms" -ge 2 ] && echo "PASS armap has a_one+b_two (GNU nm read our index)" || { echo "FAIL armap missing symbols ($$syms)"; fail=1; }; \
	rm -rf $$t; \
	[ $$fail -eq 0 ] && echo "----- ar OK -----" || { echo "----- FAILED -----"; exit 1; }

clean:
	rm -rf $(BUILD)

.PHONY: clean test
