# toolchain/cpp/Makefile — build our from-scratch C preprocessor (the `cpp`/`gcc -E` stage).
# `make` -> build/cpp ; `make test` -> preprocess real libc files with OUR cpp, feed to our cc+as ; clean.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=gnu11

OURCC := ../cc/build/cc
OURAS := ../as/build/as
GNU   ?= ../../projects/gameboy-v3/build/toolchain-gcc-initial/bin/arm-forge-linux-gnueabihf-
QEMU  ?= qemu-system-arm

$(BUILD)/cpp: cpp.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ cpp.c

$(BUILD):
	mkdir -p $(BUILD)

# The real gate: preprocess actual libc/src files with OUR cpp (no gcc -E), then compile the result with
# our cc and assemble with our as. Proves the fully self-hosted C front end: our cpp -> our cc -> our as.
# Then the string.c end-to-end (cpp+cc+as -> link -> run) must still print strlen/strcmp == 115.
test: $(BUILD)/cpp
	@$(MAKE) -s -C ../cc >/dev/null; $(MAKE) -s -C ../as >/dev/null
	@ok=0; fail=0; t=$$(mktemp -d); \
	for c in ../../libc/src/string.c ../../libc/src/stdlib.c ../../libc/src/printf.c ../../libc/src/dirent.c ../../libc/src/malloc.c; do \
	  b=$$(basename $$c); \
	  if $(BUILD)/cpp -I../../libc/include -I../../kernel/include/uapi $$c > $$t/pp.c 2>$$t/e \
	     && $(OURCC) -o $$t/pp.s $$t/pp.c 2>>$$t/e && $(OURAS) -o $$t/pp.o $$t/pp.s 2>>$$t/e; then \
	    echo "PASS $$b: our cpp -> our cc -> our as"; ok=$$((ok+1)); \
	  else echo "FAIL $$b: $$(head -1 $$t/e)"; fail=$$((fail+1)); fi; \
	done; \
	$(BUILD)/cpp -I../../libc/include -I../../kernel/include/uapi ../../libc/src/string.c > $$t/string.c 2>/dev/null; \
	$(OURCC) -o $$t/string.s $$t/string.c && $(OURAS) -o $$t/string.o $$t/string.s; \
	printf 'int strlen(char*s);int strcmp(char*a,char*b);void*malloc(int n){return 0;}\nint main(void){int n=strlen("hello");int e=strcmp("abc","abc");int d=strcmp("abd","abc");int r=n;if(e==0)r+=10;if(d>0)r+=100;return r;}\n' > $$t/main.c; \
	$(OURCC) -o $$t/main.s $$t/main.c && $(OURAS) -o $$t/main.o $$t/main.s && $(OURAS) -o $$t/crt.o ../cc/tests/crt.s; \
	$(GNU)ld -T ../cc/tests/bare.ld -o $$t/prog.elf $$t/crt.o $$t/main.o $$t/string.o 2>/dev/null; \
	timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/prog.elf >/dev/null 2>&1; g=$$?; \
	[ "$$g" = 115 ] && { echo "PASS e2e: our cpp+cc+as built libc string.c; ran = $$g"; ok=$$((ok+1)); } || { echo "FAIL e2e: got $$g want 115"; fail=$$((fail+1)); }; \
	rm -rf $$t; echo "----- $$ok passed, $$fail failed -----"; [ $$fail -eq 0 ]

clean:
	rm -rf $(BUILD)

.PHONY: clean test
