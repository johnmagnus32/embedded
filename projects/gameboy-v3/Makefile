# Makefile — gameboy-v3 PRODUCT entry point.
#
# Phase 0: a thin front-end that reads config.mk (the provider selection),
# resolves it via providers.mk, and drives the existing scripts/build.sh with the
# translated arguments. In later phases the body becomes `include ../../forge/rules.mk`
# and the shell-out disappears.
#
#   make                 # build per config.mk (default: full custom stack, MEDIA=nor)
#   make KERNEL=mainline  LIBC=musl COREUTILS=busybox BOOTLOADER=uboot   # all-OSS reference
#   make print-config    # show the resolved selection + source paths (no build)
#   make flash           # build a NOR bundle + flash + FEL-boot on the rig
#   make clean

include config.mk
include providers.mk

CFG := $(BOOTLOADER)-$(BUILD_KERNEL)-$(BUILD_ROOTFS)$(if $(LCD),-lcd_$(LCD))
BUNDLE := build/bundles/$(CFG)

# Environment build.sh reads (its own env.sh resolves the rest).
BUILD_ENV := BOOTLOADER=$(BOOTLOADER) KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) \
             MEDIA=$(MEDIA) LCD=$(LCD)

.PHONY: all build flash print-config clean test

all: build

build:
	@$(BUILD_ENV) ./scripts/build.sh

# Flash + FEL-boot on the rig (NOR only; SD is a manual dd — see build.sh MEDIA=sd).
flash: build
	@if [ "$(MEDIA)" != nor ]; then \
	  echo "make flash requires MEDIA=nor (SD is a manual dd of build/output/*-sd.img)"; exit 1; fi
	@./scripts/flash.sh $(BUNDLE) nor

# Show the resolved selection without building — the Phase-0 proof.
print-config:
	@echo "config:"
	@echo "  KERNEL     = $(KERNEL)      -> $(KERNEL_SRC)"
	@echo "  BOOTLOADER = $(BOOTLOADER)  -> $(BOOTLDR_SRC)"
	@echo "  LIBC       = $(LIBC)        -> $(LIBC_SRC)"
	@echo "  COREUTILS  = $(COREUTILS)   -> $(COREUTILS_SRC)"
	@echo "  BOARD      = $(BOARD)   MEDIA = $(MEDIA)   LCD = $(if $(LCD),$(LCD),none)"
	@echo "  -> build.sh args: KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) BOOTLOADER=$(BOOTLOADER) MEDIA=$(MEDIA)"
	@echo "  -> bundle/img cfg: $(CFG)"

clean:
	rm -rf build/bundles
	@echo "(providers' own build/ dirs are cleaned by their makefiles / scripts)"
