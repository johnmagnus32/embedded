# toolchain/cc/Makefile — build our from-scratch C compiler (a HOST program; emits ARM .s text).
# `make` -> build/cc ; `make test` -> compile+assemble+link+RUN sample programs, check results ; `make clean`.
BUILD  ?= build
CC     ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
OURLD := ../ld/build/ld
RTLIB := ../rt/build/libosrt.a   # our compiler runtime (64-bit division, int64<->FP): every test link ends with it, as gcc's end with libgcc
GNU   ?= ../../projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-
QEMU  ?= qemu-system-arm

OBJS := $(BUILD)/cc.o $(BUILD)/lex.o $(BUILD)/parse.o $(BUILD)/gen.o $(BUILD)/type.o $(BUILD)/strmap.o

$(BUILD)/cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c cc.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/strmap.o: $(COMMON)/strmap.c $(COMMON)/strmap.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Execution gate: for each tests/*.c, compile with OUR cc, assemble with OUR as, then link a tiny
# semihosting crt (tests/crt.s) that calls main() and reports its return value as the qemu exit code.
# The expected value is the number in the .c's first-line comment: `// expect: N`. OUR ld does the
# bare-metal link (-Ttext 0x40000000, virt RAM); qemu-system-arm runs it and we check the exit status.
test: $(BUILD)/cc
	@if [ ! -x "$$(command -v $(QEMU))" ]; then echo "SKIP: $(QEMU) not found"; exit 0; fi
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null; $(MAKE) -s -C ../rt >/dev/null
	@pass=0; fail=0; t=$$(mktemp -d); \
	$(OURAS) -o $$t/crt.o tests/crt.s; \
	for c in tests/*.c; do \
	  [ "$$(basename $$c)" = uart.c ] && continue; \
	  want=$$(sed -n 's|.*//[[:space:]]*expect:[[:space:]]*\([0-9]*\).*|\1|p' $$c | head -1); \
	  $(BUILD)/cc -o $$t/p.s $$c || { echo "FAIL $$c: cc errored"; fail=$$((fail+1)); continue; }; \
	  $(OURAS) -o $$t/p.o $$t/p.s || { echo "FAIL $$c: our as rejected cc output"; fail=$$((fail+1)); continue; }; \
	  $(OURLD) -Ttext 0x40000000 -o $$t/p.elf $$t/crt.o $$t/p.o $(RTLIB) 2>$$t/lderr || { echo "FAIL $$c: link: $$(cat $$t/lderr)"; fail=$$((fail+1)); continue; }; \
	  timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/p.elf >/dev/null 2>&1; got=$$?; \
	  if [ "$$got" = "$$want" ]; then echo "PASS $$(basename $$c): main() == $$got"; pass=$$((pass+1)); \
	  else echo "FAIL $$(basename $$c): got $$got, want $$want"; fail=$$((fail+1)); fi; \
	done; \
	$(BUILD)/cc -fPIC -o $$t/cd.s tests/const_data.c; \
	secs=$$(awk '/\.section|^\t\.data$$/{s=$$1 == ".data" ? ".data" : $$2} /^(k|tab|fns|cv|w):/{printf "%s=%s ", $$1, s}' $$t/cd.s); \
	case "$$secs" in "k:=.rodata tab:=.data.rel.ro,\"aw\",%progbits fns:=.data.rel.ro,\"aw\",%progbits cv:=.data w:=.data ") \
	  echo "PASS const data: .rodata / .data.rel.ro (-fPIC, addresses) / .data (volatile, non-const)"; pass=$$((pass+1));; \
	  *) echo "FAIL const data placement: $$secs"; fail=$$((fail+1));; esac; \
	$(BUILD)/cc -o $$t/u.s tests/uart.c && $(OURAS) -o $$t/u.o $$t/u.s && $(OURLD) -Ttext 0x40000000 -o $$t/u.elf $$t/crt.o $$t/u.o $(RTLIB); \
	out=$$(timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/u.elf 2>/dev/null); \
	case "$$out" in *"cc UART works"*) echo "PASS uart: cc program printed via PL011 (real I/O)"; pass=$$((pass+1));; \
	                *) echo "FAIL uart: expected UART output, got '$$out'"; fail=$$((fail+1));; esac; \
	gi=$$($(GNU)gcc -print-file-name=include); \
	$(GNU)gcc -E -Os -mcpu=cortex-a7 -marm -ffreestanding -nostdinc -isystem $$gi -I../../libc/include -I../../kernel/include/uapi ../../libc/src/string.c > $$t/str.pp.c 2>/dev/null; \
	printf '%s\n' 'int strlen(char*s);' 'int strcmp(char*a,char*b);' 'void*malloc(int n){return 0;}' \
	  'int main(void){int n=strlen("hello");int e=strcmp("abc","abc");int d=strcmp("abd","abc");int r=n;if(e==0)r+=10;if(d>0)r+=100;return r;}' > $$t/lm.c; \
	if $(BUILD)/cc -o $$t/str.s $$t/str.pp.c && $(OURAS) -o $$t/str.o $$t/str.s \
	   && $(BUILD)/cc -o $$t/lm.s $$t/lm.c && $(OURAS) -o $$t/lm.o $$t/lm.s \
	   && $(OURLD) -Ttext 0x40000000 -o $$t/lp.elf $$t/crt.o $$t/lm.o $$t/str.o $(RTLIB) 2>/dev/null; then \
	  timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/lp.elf >/dev/null 2>&1; g=$$?; \
	  [ "$$g" = 115 ] && { echo "PASS libc-string: our cc+as compiled real libc/src/string.c; strlen/strcmp ran = $$g"; pass=$$((pass+1)); } \
	                  || { echo "FAIL libc-string: got $$g, want 115"; fail=$$((fail+1)); }; \
	else echo "FAIL libc-string: compile/assemble/link of real string.c failed"; fail=$$((fail+1)); fi; \
	rm -rf $$t; echo "----- $$pass passed, $$fail failed -----"; [ $$fail -eq 0 ]

# GCC 13.3 gcc.c-torture/execute (1627 self-checking programs) through our cc/as/ld on qemu, scored only on
# tests the GNU oracle passes (torture/run.py). The suite is extracted once from the os build's gcc tarball.
GCC_TAR ?= ../../projects/gameboy-v3/image/build/downloads/gcc-13.3.0.tar.xz
CTORTURE_WORK ?= /tmp/ctorture
torture: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null
	@test -d $(CTORTURE_WORK)/gcc-13.3.0/gcc/testsuite/gcc.c-torture/execute || { \
	  test -f $(GCC_TAR) || { echo "torture: $(GCC_TAR) missing (fetch it via the os build)"; exit 1; }; \
	  mkdir -p $(CTORTURE_WORK) && tar -xJf $(GCC_TAR) -C $(CTORTURE_WORK) gcc-13.3.0/gcc/testsuite/gcc.c-torture/execute; }
	@CTORTURE_WORK=$(CTORTURE_WORK) GNU=$(abspath $(GNU)) python3 torture/run.py -j $$(nproc)

clean:
	rm -rf $(BUILD)

# AAPCS / AAPCS-VFP interop with GCC, both directions (needs the torture runtime: `make torture` first).
abi-interop: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null
	@GNU=$(abspath $(GNU)) CTORTURE_WORK=$(CTORTURE_WORK) ./torture/abi.sh

# The parser-lowered builtins (overflow arithmetic over every type combination, FP classification, abs, isdigit)
# against exact arithmetic computed in Python (torture/builtin_diff.py).
builtin-diff: $(BUILD)/cc
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../ld >/dev/null; $(MAKE) -s -C ../rt >/dev/null
	@python3 torture/builtin_diff.py

.PHONY: clean test torture abi-interop builtin-diff
