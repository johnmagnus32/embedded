# toolchain/cc/Makefile — build our from-scratch C compiler (a HOST program; emits ARM .s text).
# `make` -> build/cc ; `make test` -> compile+assemble+link+RUN sample programs, check results ; `make clean`.
BUILD  ?= build
CC     ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11

OURAS := ../as/build/as
GNU   ?= ../../projects/gameboy-v3/build/toolchain-gcc-initial/bin/arm-forge-linux-gnueabihf-
QEMU  ?= qemu-system-arm

OBJS := $(BUILD)/cc.o $(BUILD)/lex.o $(BUILD)/parse.o $(BUILD)/gen.o $(BUILD)/type.o

$(BUILD)/cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c cc.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Execution gate: for each tests/*.c, compile with OUR cc, assemble with OUR as, then link a tiny
# semihosting crt (tests/crt.s) that calls main() and reports its return value as the qemu exit code.
# The expected value is the number in the .c's first-line comment: `// expect: N`. GNU ld does the
# bare-metal link at 0x40000000 (virt RAM); qemu-system-arm runs it and we check the exit status.
test: $(BUILD)/cc
	@if [ ! -x "$$(command -v $(QEMU))" ]; then echo "SKIP: $(QEMU) not found"; exit 0; fi
	@$(MAKE) -s -C ../as >/dev/null
	@pass=0; fail=0; t=$$(mktemp -d); \
	$(OURAS) -o $$t/crt.o tests/crt.s; \
	for c in tests/*.c; do \
	  want=$$(sed -n 's|.*//[[:space:]]*expect:[[:space:]]*\([0-9]*\).*|\1|p' $$c | head -1); \
	  $(BUILD)/cc -o $$t/p.s $$c || { echo "FAIL $$c: cc errored"; fail=$$((fail+1)); continue; }; \
	  $(OURAS) -o $$t/p.o $$t/p.s || { echo "FAIL $$c: our as rejected cc output"; fail=$$((fail+1)); continue; }; \
	  $(GNU)ld -T tests/bare.ld -o $$t/p.elf $$t/crt.o $$t/p.o 2>$$t/lderr || { echo "FAIL $$c: link: $$(cat $$t/lderr)"; fail=$$((fail+1)); continue; }; \
	  timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/p.elf >/dev/null 2>&1; got=$$?; \
	  if [ "$$got" = "$$want" ]; then echo "PASS $$(basename $$c): main() == $$got"; pass=$$((pass+1)); \
	  else echo "FAIL $$(basename $$c): got $$got, want $$want"; fail=$$((fail+1)); fi; \
	done; \
	rm -rf $$t; echo "----- $$pass passed, $$fail failed -----"; [ $$fail -eq 0 ]

clean:
	rm -rf $(BUILD)

.PHONY: clean test
