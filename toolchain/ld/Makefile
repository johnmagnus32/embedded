# toolchain/ld/Makefile — build our from-scratch static linker (a HOST program, like GNU ld).
# `make` -> build/ld ; `make test` -> link our-as crt0.o + a gcc stub into a static ET_EXEC and check
# it structurally + that the R_ARM_CALL was applied ; `make clean`.
BUILD ?= build
CC    ?= cc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
COMMON := ../common
CFLAGS += -I$(COMMON)

OURAS := ../as/build/as
OURCC := ../cc/build/cc
GNU ?= ../../projects/gameboy-v3/build/toolchain-gcc-initial/bin/arm-none-linux-gnueabihf-
QEMU ?= qemu-system-arm

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
test: test-static test-pie test-script

test-static: $(BUILD)/ld
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
	printf '.text\n.global used\nused: push {lr}\nbl helper\npop {pc}\n'      > $$t/used.s; \
	printf '.text\n.global helper\nhelper: mov r0, #42\nbx lr\n'              > $$t/helper.s; \
	printf '.text\n.global unused\nunused: mov r0, #99\nbx lr\n'              > $$t/unused.s; \
	printf '.text\n.global __libc_start_main\n__libc_start_main: b used\n'    > $$t/lsm.s; \
	for x in used helper unused lsm; do $(OURAS) -o $$t/$$x.o $$t/$$x.s; done; \
	$(MAKE) -s -C ../ar >/dev/null; ../ar/build/ar rc $$t/lib.a $$t/used.o $$t/helper.o $$t/unused.o; \
	$(BUILD)/ld -o $$t/C.elf $$t/crt0.o $$t/lsm.o $$t/lib.a || { echo "FAIL: our ld errored on archive"; exit 1; }; \
	$(BUILD)/ld -o $$t/D.elf $$t/crt0.o $$t/lsm.o $$t/used.o $$t/helper.o; \
	if cmp -s $$t/C.elf $$t/D.elf; then echo "PASS archive: helper pulled, unused dropped (== direct link)"; \
	  else echo "FAIL archive: link with lib.a differs from equivalent direct link"; fail=1; fi; \
	rm -rf $$t; \
	[ $$fail -eq 0 ] && echo "----- ld M2 OK -----" || { echo "----- FAILED -----"; exit 1; }

# PIE gate (dynamic-linking M1): link a position-independent ET_DYN with our `ld -pie`, prove it carries
# the self-relocation metadata (ET_DYN + PT_DYNAMIC + R_ARM_RELATIVE in .rel.dyn), then RUN it at a
# nonzero load bias and check it self-relocated correctly. The bias is produced by a two-part boot on
# qemu -M virt: a stub (the -kernel) sets a stack and jumps to 0x40200000, where the PIE — linked at
# base 0, stripped to a raw binary — was placed via `-device loader`. tests/pie_reloc.c returns 42 only
# if the crt applied the R_ARM_RELATIVE to `p` (else `p` still holds its link-time address = garbage at
# runtime). Everything is our own: our cc + our as + our ld -pie + our crt_pie.s.
test-pie: $(BUILD)/ld
	@$(MAKE) -s -C ../as >/dev/null; $(MAKE) -s -C ../cc >/dev/null
	@fail=0; t=$$(mktemp -d); \
	$(OURAS) -o $$t/crt_pie.o tests/crt_pie.s && $(OURCC) -o $$t/pie.s tests/pie_reloc.c \
	  && $(OURAS) -o $$t/pie.o $$t/pie.s && $(BUILD)/ld -pie -o $$t/pie.elf $$t/crt_pie.o $$t/pie.o \
	  || { echo "FAIL: our cc/as/ld could not build the PIE"; rm -rf $$t; exit 1; }; \
	typ=$$($(GNU)readelf -h $$t/pie.elf | sed -n 's/.*Type:[[:space:]]*\([A-Z]*\).*/\1/p'); \
	ndyn=$$($(GNU)readelf -lW $$t/pie.elf | grep -c 'DYNAMIC'); \
	nrel=$$($(GNU)readelf -rW $$t/pie.elf | grep -c 'R_ARM_RELATIVE'); \
	[ "$$typ" = DYN ]   && echo "PASS type=DYN (ET_DYN / PIE)"                    || { echo "FAIL type=$$typ (want DYN)"; fail=1; }; \
	[ "$$ndyn" -ge 1 ]  && echo "PASS PT_DYNAMIC present"                         || { echo "FAIL no PT_DYNAMIC"; fail=1; }; \
	[ "$$nrel" -ge 1 ]  && echo "PASS $$nrel R_ARM_RELATIVE dynamic relocs"       || { echo "FAIL no R_ARM_RELATIVE relocs"; fail=1; }; \
	if [ ! -x "$$(command -v $(QEMU))" ]; then echo "SKIP run: $(QEMU) not found"; rm -rf $$t; [ $$fail -eq 0 ]; exit $$?; fi; \
	$(GNU)objcopy -O binary $$t/pie.elf $$t/blob.bin; \
	$(OURAS) -o $$t/stub.o tests/stub.s && $(BUILD)/ld -Ttext 0x40000000 -o $$t/stub.elf $$t/stub.o; \
	timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none \
	  -kernel $$t/stub.elf -device loader,file=$$t/blob.bin,addr=0x40200000 >/dev/null 2>&1; g=$$?; \
	[ "$$g" = 42 ] && echo "PASS run: PIE self-relocated at bias 0x40200000; main()==$$g" \
	              || { echo "FAIL run: got $$g, want 42 (self-relocation broken)"; fail=1; }; \
	rm -rf $$t; \
	[ $$fail -eq 0 ] && echo "----- ld PIE OK -----" || { echo "----- FAILED -----"; exit 1; }

# Linker-script gate (GNU-independent): our cc + as + `ld -T tests/prog.ld` build a program the script
# places at 0x40000000 (with script-defined __bss_start/__bss_end referenced by the code + /DISCARD/), then
# run it on qemu semihosting and check the exit code. Proves the script parser/layout/writer end-to-end.
test-script: $(BUILD)/ld
	@$(MAKE) -s -C ../cc build/cc >/dev/null; $(MAKE) -s -C ../as >/dev/null
	@if [ ! -x "$$(command -v $(QEMU))" ]; then echo "SKIP: $(QEMU) not found"; exit 0; fi
	@t=$$(mktemp -d); \
	printf '%s\n' 'extern char __bss_start[], __bss_end[];' 'int g = 5; int z[8];' \
	  'int main(void){ return (__bss_end >= __bss_start) && g==5 && z[0]==0 ? 42 : 1; }' > $$t/p.c; \
	$(OURCC) -o $$t/p.s $$t/p.c && $(OURAS) -o $$t/p.o $$t/p.s && $(OURAS) -o $$t/crt.o ../cc/tests/crt.s \
	  && $(BUILD)/ld -T tests/prog.ld -o $$t/p.elf $$t/crt.o $$t/p.o || { echo "FAIL: script link errored"; rm -rf $$t; exit 1; }; \
	timeout 10 $(QEMU) -M virt -cpu cortex-a7 -nographic -semihosting -net none -kernel $$t/p.elf >/dev/null 2>&1; g=$$?; \
	[ "$$g" = 42 ] && echo "PASS linker-script: placed + ran (__bss_start/__bss_end resolved) = $$g" \
	              || { echo "FAIL linker-script: got $$g, want 42"; rm -rf $$t; exit 1; }; \
	$(BUILD)/ld -T tests/flash.ld -o $$t/f.elf $$t/crt.o $$t/p.o || { echo "FAIL: flash.ld link errored"; rm -rf $$t; exit 1; }; \
	if command -v readelf >/dev/null 2>&1; then \
	  readelf -l $$t/f.elf | awk '/LOAD/ && $$3 ~ /^0x20000000/ && $$4 ~ /^0x080/ {ok=1} END{exit !ok}' \
	    && echo "PASS linker-script AT>: .data runs in RAM (0x20000000), loads from flash (0x08...)" \
	    || { echo "FAIL AT>: no RAM-VMA/flash-LMA LOAD segment"; rm -rf $$t; exit 1; }; \
	else echo "SKIP flash.ld AT> check (no readelf)"; fi; \
	rm -rf $$t

clean:
	rm -rf $(BUILD)

.PHONY: clean test test-static test-pie test-script
