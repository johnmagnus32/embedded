# Makefile — build the gameboy-v3 custom kernel (Stage 10).
#
# Board-selectable via BOARD=:
#   make               (BOARD=t113, default) — Allwinner T113-S3, loaded by our
#                        bootloader; 16550 UART, GIC-400 @ 0x0302xxxx.
#   make BOARD=virt    — QEMU `-M virt -cpu cortex-a7`; PL011 UART @ 0x09000000,
#                        GICv2 @ 0x0800xxxx. For actually RUNNING the kernel.
#
# The kernel takes its initramfs ONLY from the boot handoff (bootloader / QEMU
# -initrd, located via the DTB). The test rootfs is built here as a standalone
# build/initramfs.cpio.gz artifact to pass with `-initrd` (no longer embedded).
#
# Uses the pinned glibc cross toolchain (provisioned by `make toolchain` in the product);
# gen_init_cpio is an engine HOST PACKAGE (forge fetches + compiles it); the fixtures build
# gets its path via GEN_INIT_CPIO= (golden.sh passes it) — see GEN_INIT_CPIO below.

CROSS_COMPILE ?= arm-buildroot-linux-gnueabihf-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy

TARGET  := gv3kernel
BOARD   ?= t113

# Per-board build directory. The two boards differ in CFLAGS (-DBOARD_VIRT), the
# UART object, AND the link address, so their objects/binaries must NOT share a
# path — otherwise switching BOARD without `clean` silently reuses artifacts
# linked for the OTHER board (e.g. a t113 image linked at 0x41000000 booted under
# QEMU virt jumps into a zero region and hangs, producing no output). Isolating
# by directory makes `make BOARD=virt` and `make` each correct with no manual
# clean between them — which the golden test relies on.
BUILD   := build/$(BOARD)

# gen_init_cpio: the newc-cpio writer for the `fixtures` test initramfs images below.
# Only the `fixtures` goal needs it (not `all` — the kernel binary itself packs no cpio),
# so a plain `make`/`make image KERNEL=custom` never touches it. It is an ENGINE-owned
# HOST PACKAGE now (forge/hostpackages/gen_init_cpio): the forge host subsystem FETCHES +
# compiles it into <product>/build/hosttools/bin/ (`make toolchain`). The kernel provider
# does NOT own/vendor it — the golden harness (kernel/test/golden.sh) passes GEN_INIT_CPIO=
# from the product's host prefix. A bare `make -C kernel fixtures` with no override errors
# with a clear hint rather than trying to build the tool itself.
GEN_INIT_CPIO ?= gen_init_cpio-NOT-PROVISIONED

# ---- board device tree (mirror mainline: .dts in the kernel tree, we compile it) --
# The T113 board .dts + its .dtsi/dt-bindings include closure are VENDORED under
# dts/ (from mainline Linux, tag pinned in providers/kernel/mainline/provider.mk) —
# structurally identical to arch/arm/boot/dts/allwinner in the kernel tree. We
# compile them here (cpp -> dtc), exactly as the kernel's own Kbuild does, so a
# custom-kernel build produces its own board.dtb with NO mainline-Linux checkout.
#
# The console overlay (uart0-console.dtsi) is PRODUCT/BOARD data (shared with U-Boot),
# so it is NOT vendored here; the forge engine passes its path as DTB_OVERLAYS (and the
# output path as DTB_OUT) on the make line — the engine sourced board.conf cleanly in
# bash, so these arrive stripped (the kernel Makefile never parses board.conf itself).
# A bare `make -C kernel dtb` (no engine) builds the STOCK board DTB (no overlay) into
# the build dir — enough for a dev to eyeball it; the shipped DTB always comes via forge.
# The board .dts basename. The engine forwards UBOOT_BOARD_DT from board.conf; a bare
# standalone build defaults to the only board .dts we vendor.
UBOOT_BOARD_DT ?= sun8i-t113s-mangopi-mq-r-t113
DTB_BASE   := dts/allwinner/$(UBOOT_BOARD_DT).dts
DTB_OUT    ?= $(BUILD)/board.dtb
DTB_OVERLAYS ?=
# cpp include paths: the vendored allwinner/ + riscv/allwinner/ dirs (the RISC-V
# shared dies the .dts pulls in) + dts/ (for dt-bindings/*) + each overlay's own dir.
DTB_CPPFLAGS := -nostdinc -undef -x assembler-with-cpp \
                -Idts/allwinner -Idts/riscv/allwinner -Idts \
                $(foreach o,$(DTB_OVERLAYS),-I$(dir $(o)))

# -mgeneral-regs-only: never emit VFP/NEON instructions. The toolchain is
# hardfloat (-gnueabihf), so GCC would otherwise use FP registers as scratch
# even for pure-integer code (e.g. a vmov in kfdt.c) — but we never enable the
# FPU (CPACR/FPEXC), so any VFP op traps as an Undefined Instruction. Keeping the
# core register-only sidesteps that entirely (and means no FPU state to save on a
# context switch, ever). This is the same choice the Linux kernel makes.
# All headers live under include/ (Linux-style central header dir); -Iinclude
# lets the bare-name quote-includes ("proc.h") AND the uapi ones ("uapi/gv3_*.h")
# resolve. LDFLAGS points at the arch linker script.
CFLAGS  := -mcpu=cortex-a7 -marm -mgeneral-regs-only -ffreestanding -nostdlib \
           -nostartfiles -Os -Wall -Wextra -fno-builtin -fno-stack-protector \
           -Iinclude
LDFLAGS := -T arch/arm/kernel.ld -nostdlib -Wl,--build-id=none

# ---- board selection: UART driver + -DBOARD_VIRT flag -----------------------
ifeq ($(BOARD),virt)
CFLAGS  += -DBOARD_VIRT
BOARD_UART_OBJ := $(BUILD)/drivers/tty/board_pl011.o
# QEMU's raw `-kernel` loader places a 32-bit ARM image at RAM base + 0x10000;
# link there so absolute addresses are correct and QEMU's Linux boot stub can
# set r2=DTB (with linux,initrd-*) natively.
LDFLAGS += -Wl,--defsym=LOAD_ADDR=0x40010000
else
BOARD_UART_OBJ := $(BUILD)/drivers/tty/board_16550.o
endif

# Kernel objects, grouped like the source tree (arch / kernel / mm / fs /
# drivers / lib). Object paths mirror the source subdir under $(BUILD).
OBJS := $(BUILD)/arch/arm/start.o $(BUILD)/arch/arm/mmu_asm.o \
        $(BUILD)/arch/arm/vfp.o $(BUILD)/arch/arm/mmu.o \
        $(BUILD)/arch/arm/vm.o $(BUILD)/arch/arm/syscall.o \
        $(BUILD)/kernel/main.o $(BUILD)/kernel/proc.o \
        $(BUILD)/mm/pmm.o $(BUILD)/mm/kmalloc.o $(BUILD)/mm/mmap.o \
        $(BUILD)/fs/vfs.o $(BUILD)/fs/ramfs.o $(BUILD)/fs/cpio.o \
        $(BUILD)/fs/file.o $(BUILD)/fs/fs_syscall.o $(BUILD)/fs/binfmt_elf.o \
        $(BUILD)/drivers/gic.o $(BUILD)/drivers/timer.o \
        $(BUILD)/drivers/tty/uart.o $(BOARD_UART_OBJ) \
        $(BUILD)/drivers/of/kfdt.o \
        $(BUILD)/lib/libk.o $(BUILD)/lib/printf.o $(BUILD)/lib/inflate.o

# Rebuild everything if any header changes (coarse but correct for this size).
HEADERS := $(wildcard include/*.h include/uapi/*.h)

# ---- user test programs (standalone static ET_EXEC ELFs) --------------------
UCFLAGS := -mcpu=cortex-a7 -marm -mgeneral-regs-only -ffreestanding -nostdlib \
           -nostartfiles -Os -Wall -Wextra -fno-builtin -fno-stack-protector
ULDFLAGS := -T user/user.ld -nostdlib -static -Wl,--build-id=none \
            -Wl,-z,max-page-size=0x1000

.PHONY: all fixtures clean qemu test
# `all` builds ONLY the kernel binary + its board DTB — that is the deliverable the
# forge image path consumes (make image KERNEL=custom). The golden-test initramfs
# images are TEST SCAFFOLDING, not part of the kernel, so they live behind a separate
# `fixtures` goal: a plain `make` (or the engine's make-c build) no longer pays to pack
# four cpios it doesn't ship, and gen_init_cpio is only needed when you build fixtures.
all: $(BUILD)/$(TARGET).bin

# The four per-scenario test initramfs images golden.sh boots (smoke/fault/orphan/
# preempt). Built on demand: `make fixtures` or the golden harness. This is the only
# goal that pulls in gen_init_cpio.
fixtures: _require_gen_init_cpio $(BUILD)/initramfs.cpio.gz $(BUILD)/faultramfs.cpio.gz \
          $(BUILD)/orphanramfs.cpio.gz $(BUILD)/preemptramfs.cpio.gz

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.elf: user/%.c user/user.ld | $(BUILD)
	$(CC) $(UCFLAGS) $(ULDFLAGS) $< -o $@

# Preflight: fixtures need the engine-provisioned gen_init_cpio. golden.sh passes
# GEN_INIT_CPIO=<product>/build/hosttools/bin/gen_init_cpio; a bare `make fixtures` with
# no override hits the sentinel default and fails here with an actionable hint (rather
# than a cryptic "no rule to make target"). The kernel no longer compiles the tool itself.
.PHONY: _require_gen_init_cpio
_require_gen_init_cpio:
	@command -v "$(GEN_INIT_CPIO)" >/dev/null 2>&1 || [ -x "$(GEN_INIT_CPIO)" ] || { \
	  echo "kernel: gen_init_cpio not found ('$(GEN_INIT_CPIO)'). It is an engine host tool —" >&2; \
	  echo "  run 'make toolchain' in the product (or pass GEN_INIT_CPIO=<path>). golden.sh does this." >&2; \
	  exit 1; }

# ---- test initramfs (standalone artifact, passed via -initrd) ---------------
$(BUILD)/initramfs.cpio: $(BUILD)/uinit.elf $(BUILD)/uhello.elf motd.txt | _require_gen_init_cpio $(BUILD)
	@printf '%s\n' \
	  'dir /bin 0755 0 0' \
	  'dir /etc 0755 0 0' \
	  'dir /dev 0755 0 0' \
	  'nod /dev/console 0600 0 0 c 5 1' \
	  'file /init $(BUILD)/uinit.elf 0755 0 0' \
	  'file /bin/hello $(BUILD)/uhello.elf 0755 0 0' \
	  'file /etc/motd motd.txt 0644 0 0' \
	  > $(BUILD)/initramfs.list
	$(GEN_INIT_CPIO) $(BUILD)/initramfs.list > $@
	@echo "cpio: $$(wc -c < $@) bytes -> $@"

$(BUILD)/initramfs.cpio.gz: $(BUILD)/initramfs.cpio
	gzip -9 -c $< > $@
	@echo "cpio.gz: $$(wc -c < $@) bytes -> $@"

# ---- fault-isolation test initramfs -----------------------------------------
# /init forks a child that deliberately faults (/bin/fault writes through NULL);
# the parent survives and reports the killed-by-signal status. Proves the real
# fault handler kills the process instead of hanging the kernel. Used by
# `test/golden.sh fault`.
$(BUILD)/faultramfs.cpio: $(BUILD)/ufaultinit.elf $(BUILD)/ufault.elf | _require_gen_init_cpio $(BUILD)
	@printf '%s\n' \
	  'dir /bin 0755 0 0' \
	  'dir /dev 0755 0 0' \
	  'nod /dev/console 0600 0 0 c 5 1' \
	  'file /init $(BUILD)/ufaultinit.elf 0755 0 0' \
	  'file /bin/fault $(BUILD)/ufault.elf 0755 0 0' \
	  > $(BUILD)/faultramfs.list
	$(GEN_INIT_CPIO) $(BUILD)/faultramfs.list > $@
	@echo "cpio: $$(wc -c < $@) bytes -> $@"

$(BUILD)/faultramfs.cpio.gz: $(BUILD)/faultramfs.cpio
	gzip -9 -c $< > $@
	@echo "cpio.gz: $$(wc -c < $@) bytes -> $@"

# ---- orphan-reparenting test initramfs --------------------------------------
# /init forks A; A forks grandchild B then exits without waiting (orphaning B);
# init must adopt+reap B. Proves reap_current_and_switch reparents orphans to
# init instead of leaving a dangling parent PCB pointer. Used by golden.sh orphan.
$(BUILD)/orphanramfs.cpio: $(BUILD)/uorphaninit.elf | _require_gen_init_cpio $(BUILD)
	@printf '%s\n' \
	  'dir /dev 0755 0 0' \
	  'nod /dev/console 0600 0 0 c 5 1' \
	  'file /init $(BUILD)/uorphaninit.elf 0755 0 0' \
	  > $(BUILD)/orphanramfs.list
	$(GEN_INIT_CPIO) $(BUILD)/orphanramfs.list > $@
	@echo "cpio: $$(wc -c < $@) bytes -> $@"

$(BUILD)/orphanramfs.cpio.gz: $(BUILD)/orphanramfs.cpio
	gzip -9 -c $< > $@
	@echo "cpio.gz: $$(wc -c < $@) bytes -> $@"

# ---- preemption / time-slicing test initramfs -------------------------------
# /init forks two CPU-bound children that never syscall in their loops; under
# preemption the timer tick round-robins them so their markers interleave.
# Cooperative scheduling could not do this. Used by golden.sh preempt.
$(BUILD)/preemptramfs.cpio: $(BUILD)/upreemptinit.elf | _require_gen_init_cpio $(BUILD)
	@printf '%s\n' \
	  'dir /dev 0755 0 0' \
	  'nod /dev/console 0600 0 0 c 5 1' \
	  'file /init $(BUILD)/upreemptinit.elf 0755 0 0' \
	  > $(BUILD)/preemptramfs.list
	$(GEN_INIT_CPIO) $(BUILD)/preemptramfs.list > $@
	@echo "cpio: $$(wc -c < $@) bytes -> $@"

$(BUILD)/preemptramfs.cpio.gz: $(BUILD)/preemptramfs.cpio
	gzip -9 -c $< > $@
	@echo "cpio.gz: $$(wc -c < $@) bytes -> $@"

# ---- board device tree blob (dtc-compiled from the vendored dts/, + overlays) ----
# Mirrors the kernel's own Kbuild DTB rule: build a preprocessed .dts (base + an
# appended `#include "<overlay>"` per DTB_OVERLAYS, same as our mainline build.sh
# appends the console overlay), then dtc it. The forge engine sets DTB_OVERLAYS (the
# board's uart0-console.dtsi) + DTB_OUT (OUTPUT_DIR/<name>.dtb) so the composer picks
# it up; a bare `make -C kernel dtb` builds the stock board DTB (no overlay) locally.
# HOST tools (cpp/dtc), not the cross toolchain — the DTB is target-arch-neutral data.
#
# ALWAYS regenerate (dtb is .PHONY, no file-timestamp short-circuit) — exactly like our
# mainline build.sh, which restores the pristine .dts and re-applies overlays every run.
# A timestamp rule here would be WRONG: the output name (OUTPUT_DIR/<board>.dtb) is fixed
# across overlay sets, so `LCD=ili9341` on a warm tree would see the stale no-LCD DTB as
# "up to date" and ship a DTB with no panel node (and vice-versa); it also cannot see the
# .dts's transitive #include closure (dts/**/*.dtsi + dt-bindings/*), so a re-synced .dtsi
# wouldn't rebuild. dtc is fast + deterministic, so unconditional rebuild is the safe,
# faithful choice (verified byte-identical to the mainline-built DTB for a given overlay set).
.PHONY: dtb
dtb: | $(BUILD)
	@mkdir -p $(dir $(DTB_OUT))
	@cp $(DTB_BASE) $(BUILD)/board.dts
	@for ov in $(DTB_OVERLAYS); do printf '\n#include "%s"\n' "$$(basename $$ov)" >> $(BUILD)/board.dts; \
	  cp $$ov $(BUILD)/; done
	cpp $(DTB_CPPFLAGS) -I$(BUILD) $(BUILD)/board.dts -o $(BUILD)/board.pp.dts
	dtc -I dts -O dtb -o $(DTB_OUT) $(BUILD)/board.pp.dts
	@echo "dtb: $$(wc -c < $(DTB_OUT)) bytes -> $(DTB_OUT)$(if $(DTB_OVERLAYS), (overlays: $(notdir $(DTB_OVERLAYS))),)"

# Sources now live in subdirs (arch/arm, kernel, mm, fs, drivers/*, lib); the %
# captures the full relative path (e.g. kernel/main), so mkdir -p the object's
# parent dir before compiling.
$(BUILD)/%.o: %.S | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c $(HEADERS) | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) arch/arm/kernel.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo
	@echo "Built $@ (BOARD=$(BOARD))."
	@$(CROSS_COMPILE)size $(BUILD)/$(TARGET).elf

# ---- run under QEMU (requires: make BOARD=virt) -----------------------------
# Boot the RAW .bin (not the ELF): that makes QEMU run its Linux boot stub, which
# loads the image at 0x40010000 (matches our LOAD_ADDR), passes the DTB in r2,
# and populates /chosen/linux,initrd-start|end so the kernel finds the -initrd.
qemu: $(BUILD)/$(TARGET).bin $(BUILD)/initramfs.cpio.gz
	qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -net none \
	  -kernel $(BUILD)/$(TARGET).bin -initrd $(BUILD)/initramfs.cpio.gz

# ---- golden regression test (the safety net) --------------------------------
# Boots the kernel under QEMU and checks program output against known-good
# markers (built-in smoke test always; real BusyBox if the rootfs image exists).
# Run this after every change that should be behavior-neutral. The script builds
# BOARD=virt itself, so this target carries no build prereqs. See test/golden.sh.
test:
	./test/golden.sh

clean:
	rm -rf build
