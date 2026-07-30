# forge/rules.mk — THE SHARED BUILD ENGINE (top-level targets + dependency graph).
#
# A product's Makefile is a thin `include $(REPO_ROOT)/forge/rules.mk` after it
# sets its selection (config.mk). This file expresses the build as a real Make
# dependency graph — replacing the implicit ordering that used to be encoded in
# the `NN-` script filename prefixes — and delegates each layer to the proven
# build backend (the 0N-*.sh scripts + provider Makefiles). Reimplementing the
# reproducible fetch/verify/toolchain logic in Make would be a large rewrite for
# no behavior gain; the engine's job is orchestration + reuse, not re-doing that.
#
# Targets:
#   image   (default) build the product for its MEDIA: nor -> a flash bundle,
#                     sd -> a dd-able .img
#   flash             build (nor) + flash + FEL-boot on the rig
#   kernel bootloader rootfs toolchain   individual layers
#   print-config      show the resolved selection (no build)
#   test              run the kernel golden QEMU tests
#   clean

# The product must set these before including us (config.mk):
#   KERNEL BOOTLOADER LIBC COREUTILS BOARD MEDIA LCD  and  PRODUCT_DIR
PRODUCT_DIR ?= $(CURDIR)
BUILD       := $(PRODUCT_DIR)/build
SCRIPTS     := $(PRODUCT_DIR)/scripts

include $(dir $(lastword $(MAKEFILE_LIST)))providers.mk

OUTPUT := $(BUILD)/output
BUNDLE := $(BUILD)/bundles/$(CFG)

# Environment the shell backends (0N-*.sh / build.sh / flash.sh) read.
BACKEND_ENV := BOOTLOADER=$(BOOTLOADER) KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) \
               MEDIA=$(MEDIA) LCD=$(LCD)

.PHONY: image flash toolchain kernel bootloader rootfs print-config test clean

# --- the dependency graph -----------------------------------------------------
# `image` depends on the three component layers + the assembler; each layer
# depends on the toolchain. Make orders them from the deps, not from NN- names.
image: kernel bootloader rootfs
	@$(MAKE) --no-print-directory -f $(dir $(lastword $(MAKEFILE_LIST)))image.mk \
	   BACKEND_ENV="$(BACKEND_ENV)" MEDIA=$(MEDIA) CFG=$(CFG) BUNDLE=$(BUNDLE) \
	   PRODUCT_DIR=$(PRODUCT_DIR) SCRIPTS=$(SCRIPTS) assemble

toolchain:
	@$(SCRIPTS)/00-toolchain.sh

# Each layer delegates to its backend (forge/<layer>.mk), which shells the proven
# builder. Ordered after toolchain via a normal prerequisite.
kernel bootloader rootfs: toolchain
	@$(MAKE) --no-print-directory -f $(dir $(lastword $(MAKEFILE_LIST)))$@.mk \
	   BACKEND_ENV="$(BACKEND_ENV)" PRODUCT_DIR=$(PRODUCT_DIR) SCRIPTS=$(SCRIPTS) \
	   KERNEL=$(KERNEL) BOOTLOADER=$(BOOTLOADER) LIBC=$(LIBC) COREUTILS=$(COREUTILS) \
	   BUILD_KERNEL=$(BUILD_KERNEL) BUILD_ROOTFS=$(BUILD_ROOTFS) LCD=$(LCD) \
	   KERNEL_SRC=$(KERNEL_SRC) BOOTLDR_SRC=$(BOOTLDR_SRC) build

flash: image
	@if [ "$(MEDIA)" != nor ]; then \
	  echo "make flash needs MEDIA=nor (SD is a manual dd of build/output/*-sd.img)"; exit 1; fi
	@$(SCRIPTS)/flash.sh $(BUNDLE) nor

print-config:
	@echo "forge config:"
	@echo "  KERNEL     = $(KERNEL)      -> $(KERNEL_SRC)"
	@echo "  BOOTLOADER = $(BOOTLOADER)  -> $(BOOTLDR_SRC)"
	@echo "  LIBC       = $(LIBC)        -> $(LIBC_SRC)"
	@echo "  COREUTILS  = $(COREUTILS)   -> $(COREUTILS_SRC)"
	@echo "  BOARD=$(BOARD)  MEDIA=$(MEDIA)  LCD=$(if $(LCD),$(LCD),none)"
	@echo "  backend args: KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) BOOTLOADER=$(BOOTLOADER)"
	@echo "  cfg: $(CFG)"

test:
	@$(REPO_ROOT)/kernel/test/golden.sh

clean:
	rm -rf $(BUILD)/bundles
	@echo "(provider build/ dirs cleaned by their own makefiles)"
