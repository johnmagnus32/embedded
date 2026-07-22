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
# Uses the pinned glibc cross toolchain (../scripts/00-toolchain.sh);
# gen_init_cpio comes from ../scripts/02-kernel.sh.

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

GEN_INIT_CPIO ?= ../build/linux/usr/gen_init_cpio

# -mgeneral-regs-only: never emit VFP/NEON instructions. The toolchain is
# hardfloat (-gnueabihf), so GCC would otherwise use FP registers as scratch
# even for pure-integer code (e.g. a vmov in kfdt.c) — but we never enable the
# FPU (CPACR/FPEXC), so any VFP op traps as an Undefined Instruction. Keeping the
# core register-only sidesteps that entirely (and means no FPU state to save on a
# context switch, ever). This is the same choice the Linux kernel makes.
CFLAGS  := -mcpu=cortex-a7 -marm -mgeneral-regs-only -ffreestanding -nostdlib \
           -nostartfiles -Os -Wall -Wextra -fno-builtin -fno-stack-protector
LDFLAGS := -T kernel.ld -nostdlib -Wl,--build-id=none

# ---- board selection: UART driver + -DBOARD_VIRT flag -----------------------
ifeq ($(BOARD),virt)
CFLAGS  += -DBOARD_VIRT
BOARD_UART_OBJ := $(BUILD)/board_pl011.o
# QEMU's raw `-kernel` loader places a 32-bit ARM image at RAM base + 0x10000;
# link there so absolute addresses are correct and QEMU's Linux boot stub can
# set r2=DTB (with linux,initrd-*) natively.
LDFLAGS += -Wl,--defsym=LOAD_ADDR=0x40010000
else
BOARD_UART_OBJ := $(BUILD)/board_16550.o
endif


OBJS := $(BUILD)/start.o $(BUILD)/kmain.o $(BUILD)/gic.o \
        $(BUILD)/timer.o $(BUILD)/uart.o $(BOARD_UART_OBJ) $(BUILD)/printf.o \
        $(BUILD)/vfp.o \
        $(BUILD)/pmm.o $(BUILD)/mmu.o $(BUILD)/mmu_asm.o \
        $(BUILD)/syscall.o $(BUILD)/vm.o $(BUILD)/proc.o \
        $(BUILD)/elf.o $(BUILD)/libk.o $(BUILD)/kmalloc.o \
        $(BUILD)/ramfs.o $(BUILD)/cpio.o $(BUILD)/file.o \
        $(BUILD)/vfs.o $(BUILD)/fs_syscall.o $(BUILD)/mm_syscall.o \
        $(BUILD)/kfdt.o $(BUILD)/inflate.o

HEADERS := uart.h gic.h timer.h pmm.h mmu.h syscall.h vm.h proc.h elf.h \
           libk.h kmalloc.h ramfs.h cpio.h file.h vfs.h fs_syscall.h \
           fs_abi.h kstat.h dirent.h mm_syscall.h kfdt.h inflate.h board.h

# ---- user test programs (standalone static ET_EXEC ELFs) --------------------
UCFLAGS := -mcpu=cortex-a7 -marm -mgeneral-regs-only -ffreestanding -nostdlib \
           -nostartfiles -Os -Wall -Wextra -fno-builtin -fno-stack-protector
ULDFLAGS := -T user/user.ld -nostdlib -static -Wl,--build-id=none \
            -Wl,-z,max-page-size=0x1000

.PHONY: all clean qemu test
all: $(BUILD)/$(TARGET).bin $(BUILD)/initramfs.cpio.gz $(BUILD)/faultramfs.cpio.gz \
     $(BUILD)/orphanramfs.cpio.gz $(BUILD)/preemptramfs.cpio.gz

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.elf: user/%.c user/user.ld | $(BUILD)
	$(CC) $(UCFLAGS) $(ULDFLAGS) $< -o $@

# ---- test initramfs (standalone artifact, passed via -initrd) ---------------
$(BUILD)/initramfs.cpio: $(BUILD)/uinit.elf $(BUILD)/uhello.elf motd.txt | $(BUILD)
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
$(BUILD)/faultramfs.cpio: $(BUILD)/ufaultinit.elf $(BUILD)/ufault.elf | $(BUILD)
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
$(BUILD)/orphanramfs.cpio: $(BUILD)/uorphaninit.elf | $(BUILD)
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
$(BUILD)/preemptramfs.cpio: $(BUILD)/upreemptinit.elf | $(BUILD)
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

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) kernel.ld
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
