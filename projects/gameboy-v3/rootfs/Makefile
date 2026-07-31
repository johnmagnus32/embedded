# Makefile — build gv3libc and the from-scratch rootfs programs.
#
# Phase 1 (current): STATIC libc (libc.a), programs linked as static ARM ET_EXEC
# at 0x10000, runnable on our custom kernel. Uses the SAME musl cross toolchain
# as the reference rootfs, but -nostdlib: we provide our OWN crt0 + libc, so musl
# is only the compiler/assembler/linker driver, not its libc. (Building with the
# glibc toolchain would work too; the point is we link NONE of their libc.)
#
#   make            # build libc.a + all programs into build/
#   make clean

CROSS_COMPILE ?= arm-buildroot-linux-musleabihf-
CC      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy

# Phase 1 of the forge refactor: libc + coreutils graduated to top-level PROVIDERS
# at the repo root; the kernel too. This Makefile is now the rootfs ASSEMBLER — it
# builds those providers' sources (still via this file for now; Phase 2 moves the
# assembly into forge/rootfs.mk) and stages them into an initramfs. Paths reach UP
# to the repo root (rootfs/ is at projects/gameboy-v3/rootfs -> 3 levels down).
# Provider homes at the repo root (no trailing-comment whitespace — it would leak
# into the value and break $(wildcard)/globs).
REPO_ROOT := $(abspath ../../..)
BUILD := build
LIBC  := $(REPO_ROOT)/libc
COREUTILS := $(REPO_ROOT)/coreutils
KERNEL_PROVIDER := $(REPO_ROOT)/kernel
# Product overlay (forge refactor Phase 3): files SHIPPED into the rootfs, layered
# on the generic staged tree. rootfs/ sits under the product, so `..` is it.
PRODUCT_DIR := $(abspath ..)
OVERLAY     := $(PRODUCT_DIR)/overlay

# The kernel owns the UAPI (single source of truth): syscall numbers
# (gv3_syscalls.h) AND the ABI types/constants (gv3_abi.h). We install COPIES of
# both into a STAGED include dir under build/ (the `make headers_install` model)
# so the libc builds standalone against a snapshot rather than reaching into
# ../kernel live. Staging into build/ (not the source tree) keeps libc/include/
# pure source and makes `make clean` a single `rm -rf build`.
KERNEL_UAPI_DIR := $(KERNEL_PROVIDER)/include/uapi
UAPI_HDRS       := gv3_syscalls.h gv3_abi.h
STAGE_INC       := $(BUILD)/include
UAPI_DST        := $(patsubst %,$(STAGE_INC)/%,$(UAPI_HDRS))

# BOARD selects the target. `virt` (QEMU -M virt) must avoid VFP, matching the
# kernel's own user programs (-mgeneral-regs-only) — the kernel enables VFP
# carefully and the test harness runs -M virt. Default (t113) allows VFP.
#   make               # t113 build
#   make BOARD=virt    # QEMU virt build (VFP-free)
BOARD ?= t113
ifeq ($(BOARD),virt)
ARCH_FLAGS := -mcpu=cortex-a7 -marm -mgeneral-regs-only
else
ARCH_FLAGS := -mcpu=cortex-a7 -marm
endif

# -ffreestanding + -nostdlib: no host/musl libc, no crt. -fno-builtin so the
# compiler doesn't assume libc semantics; we still provide memcpy/memset because
# GCC may emit calls to them. -fno-stack-protector: no __stack_chk_guard.
# Include order: our libc headers, then the staged UAPI (installed from kernel),
# then src (for private headers).
CFLAGS := $(ARCH_FLAGS) -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-stack-protector -Os -Wall -Wextra \
          -I$(LIBC)/include -I$(STAGE_INC) -I$(LIBC)/src

# ---- LINK: static (default) vs dynamic ------------------------------------- *
# STATIC  (Phase 1): libc.a's code is copied into each binary — self-contained,
#   boots on our kernel today. This is the default.
# DYNAMIC (preview): build a PIC libc.so and link programs against it, recording
#   DT_NEEDED=libc.so + PT_INTERP=/lib/ld-gv3.so.1. These binaries are for SEEING
#   THE STRUCTURE (readelf shows the dynamic section, interpreter, GOT/PLT, and
#   they shrink). They will NOT RUN until (a) we write that dynamic linker and
#   (b) the kernel gains PT_INTERP loading + file-backed mmap. See README.
#     make                # static
#     make LINK=dynamic   # dynamic preview (toggle -> `make clean` first)
LINK      ?= static
DYNLINKER := /lib/ld-gv3.so.1
ifeq ($(LINK),dynamic)
  PICFLAGS := -fPIC
  LIBDEP   := $(BUILD)/libc.so
  LINKLIB  := -L$(BUILD) -lc
  # default linker script (emits .interp/.dynamic/.got/.plt); non-PIE ET_EXEC.
  PROG_LDFLAGS := -nostdlib -no-pie -Wl,--dynamic-linker=$(DYNLINKER) \
                  -Wl,--build-id=none
else
  PICFLAGS :=
  LIBDEP   := $(BUILD)/libc.a
  LINKLIB  := $(BUILD)/libc.a
  PROG_LDFLAGS := -T $(LIBC)/user.ld -nostdlib -static -Wl,--build-id=none \
                  -Wl,-z,max-page-size=0x1000
endif

# ---- libc object list (auto-discovered) -------------------------------------
# Every .c under libc/src is a libc unit — no hand-maintained list. crt0 is
# handled SEPARATELY (below): _start belongs in the executable, never in the
# library (a .so must not own the program entry point).
# libc sources live in the provider (absolute path); build objects under a clean
# $(BUILD)/libc/ prefix keyed on the bare filename (via vpath), so the object
# paths don't embed the absolute source path.
LIBC_SRCS := $(wildcard $(LIBC)/src/*.c)
LIBC_OBJS := $(patsubst %.c,$(BUILD)/libc/%.c.o,$(notdir $(LIBC_SRCS)))
CRT_OBJ   := $(BUILD)/libc/crt0.S.o
vpath %.c $(LIBC)/src
vpath %.S $(LIBC)/src/crt

# ---- programs (auto-discovered: one coreutils/*.c -> one static ELF) ---------
# Adding a coreutil is a one-file drop into the coreutils/ provider — no Makefile
# edit. /init is a SHELL SCRIPT (init.sh), not a compiled program, so it isn't here.
PROGS      := $(notdir $(patsubst %.c,%,$(wildcard $(COREUTILS)/*.c)))
PROG_ELVES := $(patsubst %,$(BUILD)/bin/%,$(PROGS))
# find coreutils sources by bare name (the provider dir is an absolute path)
vpath %.c $(COREUTILS)

# Programs that are DEMOS/tests, not shipped in the rootfs image (still built by
# `make all` so they can be run standalone, just excluded from the cpio).
DEMO_PROGS  := hello
SHIP_PROGS  := $(filter-out $(DEMO_PROGS),$(PROGS))

# gen_init_cpio host tool (built by the kernel step; falls back to $PATH)
GEN_INIT_CPIO ?= $(REPO_ROOT)/projects/gameboy-v3/build/linux/usr/gen_init_cpio

# The staging tree: a real directory that MIRRORS the final "/". We install the
# programs + init into it, then WALK it to generate the cpio listing (plus the
# separate device table). This is the buildroot/yocto/kernel pattern.
STAGE := $(BUILD)/rootfs

.PHONY: all clean headers rootfs qemu
all: headers $(PROG_ELVES)
	@echo
	@$(CROSS_COMPILE)size $(PROG_ELVES)

# ---- populate the staging tree (mirrors the final "/") ----------------------
# Two layers, buildroot-style: (1) stage the GENERIC files this assembler builds
# (the shipped programs); (2) MERGE the product's overlay/ on top (its PID-1 init
# + any etc/... it ships). This is how "generic rootfs + this product's policy"
# compose without forking the rootfs. init.sh -> /init (the shebang PID-1);
# init.busybox is the OTHER backend's (03-rootfs.sh) PID-1, so it's excluded here.
# .PHONY because it's simpler to always re-sync than to track per-file deps; the
# programs it copies already carry the real dependency graph.
.PHONY: stage
stage: $(patsubst %,$(BUILD)/bin/%,$(SHIP_PROGS)) $(OVERLAY)/init.sh $(if $(filter dynamic,$(LINK)),$(BUILD)/libc.so)
	@mkdir -p $(STAGE)/bin
	@for p in $(SHIP_PROGS); do install -m 0755 $(BUILD)/bin/$$p $(STAGE)/bin/$$p; done
	@# overlay-merge: layer product files on top of the generic staged tree.
	@# init.sh becomes /init; init.busybox belongs to the busybox backend (skip).
	@for f in $$(cd $(OVERLAY) && find . -type f ! -name init.busybox | sed 's|^\./||'); do \
	  dst="$(STAGE)/$$f"; [ "$$f" = init.sh ] && dst="$(STAGE)/init"; \
	  mkdir -p "$$(dirname "$$dst")"; install -m 0755 "$(OVERLAY)/$$f" "$$dst"; \
	done
	@# dynamic build: ship the real ld-gv3.so.1 (the dynamic linker, ld/Makefile)
	@# and libc.so into /lib at the paths the programs' PT_INTERP + DT_NEEDED record.
	@if [ "$(LINK)" = "dynamic" ]; then \
	  $(MAKE) -C $(LIBC)/ld BOARD=$(BOARD) >/dev/null 2>&1 || exit 1; \
	  mkdir -p $(STAGE)/lib; \
	  install -m 0755 $(BUILD)/ld-gv3.so.1 $(STAGE)/lib/ld-gv3.so.1; \
	  install -m 0755 $(BUILD)/libc.so $(STAGE)/lib/libc.so; \
	fi

# ---- package the staging tree into a bootable initramfs (root-owned cpio) ----
# Generate the gen_init_cpio listing by WALKING the staging tree (dirs + files,
# all uid/gid 0), then append the device table (the only hand-maintained part —
# /dev nodes can't exist as real host files). Mirrors forge/backends/rootfs.sh.
rootfs: $(BUILD)/rootfs.cpio.gz
$(BUILD)/rootfs.cpio.gz: stage rootfs.devs
	@{ \
	  grep -v '^[[:space:]]*#' rootfs.devs | grep -v '^[[:space:]]*$$'; \
	  cd $(STAGE); \
	  find . -mindepth 1 -path ./dev -prune -o -print | while read -r p; do \
	    rel="$${p#.}"; \
	    if [ -L "$$p" ]; then printf 'slink %s %s 0777 0 0\n' "$$rel" "$$(readlink "$$p")"; \
	    elif [ -d "$$p" ]; then printf 'dir %s 0%s 0 0\n'  "$$rel" "$$(stat -c '%a' "$$p")"; \
	    elif [ -f "$$p" ]; then printf 'file %s %s 0%s 0 0\n' "$$rel" "$(CURDIR)/$(STAGE)$$rel" "$$(stat -c '%a' "$$p")"; \
	    fi; \
	  done; \
	} > $(BUILD)/rootfs.list
	$(GEN_INIT_CPIO) $(BUILD)/rootfs.list | gzip -9 > $@
	@echo "  CPIO $@ ($$(wc -c < $@) bytes)"

# ---- boot the rootfs on the custom kernel under QEMU virt -------------------
# Requires a virt kernel: (cd ../kernel && make BOARD=virt). This target builds
# the rootfs BOARD=virt automatically via the recursive call in its prereq.
KERNEL_VIRT := $(KERNEL_PROVIDER)/build/virt/gv3kernel.bin
qemu: $(BUILD)/rootfs.cpio.gz
	qemu-system-arm -M virt -cpu cortex-a7 -m 128M -nographic -net none \
	  -kernel $(KERNEL_VIRT) -initrd $(BUILD)/rootfs.cpio.gz

# install the kernel's UAPI headers into the staged include dir (headers_install
# model). Generated artifacts, so they live under build/, never in the source tree.
headers: $(UAPI_DST)
$(STAGE_INC)/%.h: $(KERNEL_UAPI_DIR)/%.h | $(BUILD)
	@cp $< $@
	@echo "  UAPI  $< -> $@"

$(BUILD):
	mkdir -p $(BUILD)/libc $(BUILD)/bin $(STAGE_INC)

# assemble/compile every libc unit. PICFLAGS is empty for static, -fPIC for
# dynamic (position-independent so the .so works at any load address). Order-only
# dep on the installed UAPI header so it's present before anything includes it.
$(BUILD)/libc/%.S.o: %.S | $(BUILD) $(UAPI_DST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@
$(BUILD)/libc/%.c.o: %.c | $(BUILD) $(UAPI_DST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# the static library (archive of .o — code copied into each program at link time)
$(BUILD)/libc.a: $(LIBC_OBJS)
	$(AR) rcs $@ $^
	@echo "  AR  $@"

# the SHARED library (PIC .so — one copy, mapped into each program at RUN time by
# the dynamic linker). SONAME is the name programs record as DT_NEEDED. crt0 is
# NOT in here — the .so has no program entry point.
$(BUILD)/libc.so: $(LIBC_OBJS)
	$(CC) $(CFLAGS) $(PICFLAGS) -shared -nostdlib -Wl,--build-id=none \
	  -Wl,-soname,libc.so $(LIBC_OBJS) -o $@
	@echo "  SO  $@"

# link a program. crt0.o (the entry point) is always linked into the EXECUTABLE.
# static: our user.ld + libc.a copied in. dynamic: default script + -lc (records
# DT_NEEDED) + the interpreter path (PT_INTERP).
$(BUILD)/bin/%: %.c $(CRT_OBJ) $(LIBDEP) | $(BUILD) $(UAPI_DST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(PICFLAGS) $(PROG_LDFLAGS) $(CRT_OBJ) $< $(LINKLIB) -o $@
	@echo "  LD  $@  [$(LINK)]  ($$($(CROSS_COMPILE)readelf -h $@ | grep Type | tr -s ' ' | cut -d' ' -f3))"

clean:
	rm -rf $(BUILD)     # includes the staged UAPI snapshot ($(STAGE_INC))
