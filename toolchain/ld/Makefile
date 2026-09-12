# toolchain/ld/Makefile — build our from-scratch static linker (a HOST program, like GNU ld).
# `make` -> build/ld ; `make test` -> link our-as crt0.o + a gcc stub into a static ET_EXEC and check
# it structurally + that the R_ARM_CALL was applied ; `make clean`.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
GNU ?= ../../projects/gameboy-v3/build/toolchain-gcc-initial/bin/arm-forge-linux-gnueabihf-

# Three layers, like the assembler: front-end (ld.c) + object-format backend (elf.c) + machine-dependent
# backend (arm.c). A new arch swaps arm.c; a new object format swaps elf.c.
OBJS := $(BUILD)/ld.o $(BUILD)/elf.o $(BUILD)/arm.o $(BUILD)/elfutil.o

$(BUILD)/ld: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c ld.h $(COMMON)/elf.h $(COMMON)/elfutil.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/elfutil.o: $(COMMON)/elfutil.c $(COMMON)/elfutil.h $(COMMON)/elf.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# M2 gate (qemu-arm user-mode isn't packaged here, so this is structural — mirrors how the assembler's
# gate was "GNU ld accepts our .o"): link crt0 + a stub touching all four section classes (.text/.rodata/
# .data/.bss) and prove our ld emits a valid ET_EXEC with the M2 properties: entry=_start, TWO PT_LOADs
# split W^X (seg0 R-X, seg1 R-W, never R-W-X), a .bss whose memsz exceeds its filesz, and the R_ARM_CALL
# placeholder (0xebfffffe) relocated to a real branch. Entry = LOAD_BASE + ehdr(52) + 2*phdr(64) = 0x10074.
test: $(BUILD)/ld
	@if [ ! -x "$(GNU)gcc" ]; then echo "SKIP: GNU gcc not found at '$(GNU)gcc' (set GNU=<prefix>)"; exit 0; fi
	@$(MAKE) -s -C ../as >/dev/null                 # ensure our assembler is built
	@t=$$(mktemp -d); fail=0; \
	$(OURAS) -o $$t/crt0.o ../../libc/src/crt/crt0.S; \
	printf '%s\n' \
	  'static const char msg[] = "hi rodata";      /* .rodata */' \
	  'int g_data = 0x1234;                         /* .data   */' \
	  'int g_bss[16];                               /* .bss    */' \
	  'static void sys_exit(int c){ register int r0 asm("r0")=c; register int r7 asm("r7")=1;' \
	  '  asm volatile("svc #0"::"r"(r0),"r"(r7)); }' \
	  'void __libc_start_main(long *sp){ g_bss[0]=g_data+msg[0]; sys_exit((int)sp[0]+g_bss[0]); }' > $$t/stub.c; \
	$(GNU)gcc -c -Os -mcpu=cortex-a7 -marm -ffreestanding -nostdlib -o $$t/stub.o $$t/stub.c; \
	$(BUILD)/ld -o $$t/prog.elf $$t/crt0.o $$t/stub.o || { echo "FAIL: our ld errored"; exit 1; }; \
	typ=$$($(GNU)readelf -h $$t/prog.elf | sed -n 's/.*Type:[[:space:]]*\([A-Z]*\).*/\1/p'); \
	ent=$$($(GNU)readelf -h $$t/prog.elf | sed -n 's/.*Entry point address:[[:space:]]*//p'); \
	nseg=$$($(GNU)readelf -l $$t/prog.elf | grep -c 'LOAD'); \
	segs=$$($(GNU)readelf -lW $$t/prog.elf | awk '/LOAD/{f=""; for(i=7;i<NF;i++) if($$i ~ /^[RWEX]+$$/) f=f $$i; print $$5,$$6,f}'); \
	f0=$$(echo "$$segs" | sed -n 1p | cut -d' ' -f3); \
	f1=$$(echo "$$segs" | sed -n 2p | cut -d' ' -f3); \
	bfsz=$$(echo "$$segs" | sed -n 2p | cut -d' ' -f1); bmsz=$$(echo "$$segs" | sed -n 2p | cut -d' ' -f2); \
	bl=$$(od -An -tx4 -j124 -N4 $$t/prog.elf | tr -d ' \n'); \
	[ "$$typ" = EXEC ]      && echo "PASS type=EXEC"                          || { echo "FAIL type=$$typ (want EXEC)"; fail=1; }; \
	[ "$$ent" = 0x10074 ]   && echo "PASS entry=$$ent (_start)"               || { echo "FAIL entry=$$ent (want 0x10074)"; fail=1; }; \
	[ "$$nseg" = 2 ]        && echo "PASS two PT_LOAD segments"               || { echo "FAIL $$nseg PT_LOAD (want 2)"; fail=1; }; \
	[ "$$f0" = "RE" ]       && echo "PASS seg0 R-X (W^X: not writable)"       || { echo "FAIL seg0 flags=$$f0 (want R E)"; fail=1; }; \
	[ "$$f1" = "RW" ]       && echo "PASS seg1 R-W (W^X: not executable)"     || { echo "FAIL seg1 flags=$$f1 (want RW)"; fail=1; }; \
	[ "$$bmsz" != "$$bfsz" ] && echo "PASS .bss memsz $$bmsz > filesz $$bfsz" || { echo "FAIL seg1 memsz=$$bmsz filesz=$$bfsz (want memsz>filesz for .bss)"; fail=1; }; \
	case "$$bl" in ebfffffe) echo "FAIL bl not relocated (still placeholder)"; fail=1;; \
	               eb??????) echo "PASS bl relocated -> 0x$$bl (R_ARM_CALL applied)";; \
	               *) echo "FAIL bl word 0x$$bl not a bl"; fail=1;; esac; \
	$(GNU)readelf -lW $$t/prog.elf | sed -n '/LOAD/p' | sed 's/^/    /'; \
	rm -rf $$t; \
	[ $$fail -eq 0 ] && echo "----- ld M2 OK -----" || { echo "----- FAILED -----"; exit 1; }

clean:
	rm -rf $(BUILD)

.PHONY: clean test
