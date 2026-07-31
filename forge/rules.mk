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

include $(dir $(lastword $(MAKEFILE_LIST)))providers.mk

# The generic build backends (fetch/build recipes + mechanism) live WITH the
# engine, not inside the product (forge refactor Phase 6). BACKENDS is where the
# 0N-equivalent scripts + lib.sh live; TOOLS is the repo-root rig/dev tooling.
BACKENDS := $(FORGE_DIR)/backends
TOOLS    := $(REPO_ROOT)/tools

OUTPUT := $(BUILD)/output
BUNDLE := $(BUILD)/bundles/$(CFG)

# Environment the shell backends read. They source forge/backends/lib.sh, which
# needs PRODUCT_DIR + BOARD_NAME to resolve the product's data (versions.env,
# board/*/{board.env,layout.env,board.mk}); pass both into every backend call.
BACKEND_ENV := PRODUCT_DIR=$(PRODUCT_DIR) BOARD_NAME=$(BOARD) \
               BOOTLOADER=$(BOOTLOADER) KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) \
               MEDIA=$(MEDIA) LCD=$(LCD)

.PHONY: image flash toolchain kernel bootloader rootfs print-config test clean

# --- the dependency graph -----------------------------------------------------
# `image` depends on the three component layers + the assembler; each layer
# depends on the toolchain. Make orders them from the deps, not from NN- names.
image: kernel bootloader rootfs
	@$(MAKE) --no-print-directory -f $(FORGE_DIR)/image.mk \
	   BACKEND_ENV="$(BACKEND_ENV)" BACKENDS=$(BACKENDS) MEDIA=$(MEDIA) CFG=$(CFG) BUNDLE=$(BUNDLE) \
	   PRODUCT_DIR=$(PRODUCT_DIR) BOARD_NAME=$(BOARD) assemble

toolchain:
	@$(BACKEND_ENV) $(BACKENDS)/toolchain.sh

# Each layer delegates to its backend (forge/<layer>.mk), which shells the proven
# builder in forge/backends/. Ordered after toolchain via a normal prerequisite.
kernel bootloader rootfs: toolchain
	@$(MAKE) --no-print-directory -f $(FORGE_DIR)/$@.mk \
	   BACKEND_ENV="$(BACKEND_ENV)" BACKENDS=$(BACKENDS) \
	   PRODUCT_DIR=$(PRODUCT_DIR) BOARD_NAME=$(BOARD) \
	   KERNEL=$(KERNEL) BOOTLOADER=$(BOOTLOADER) LIBC=$(LIBC) COREUTILS=$(COREUTILS) \
	   BUILD_KERNEL=$(BUILD_KERNEL) BUILD_ROOTFS=$(BUILD_ROOTFS) LCD=$(LCD) \
	   KERNEL_TARGET=$(KERNEL_TARGET) ROOTFS_TARGET=$(ROOTFS_TARGET) \
	   KERNEL_SRC=$(KERNEL_SRC) BOOTLDR_SRC=$(BOOTLDR_SRC) build

flash: image
	@if [ "$(MEDIA)" != nor ]; then \
	  echo "make flash needs MEDIA=nor (SD is a manual dd of build/output/*-sd.img)"; exit 1; fi
	@$(TOOLS)/flash.sh $(BUNDLE) nor

print-config:
	@echo "forge config:"
	@printf '  %-10s = %-8s -> %s\n' KERNEL     "$(KERNEL)"     "$(KERNEL_SRC)"
	@printf '  %-10s = %-8s -> %s\n' BOOTLOADER "$(BOOTLOADER)" "$(BOOTLDR_SRC)"
	@printf '  %-10s = %-8s -> %s\n' LIBC       "$(LIBC)"       "$(LIBC_SRC)"
	@printf '  %-10s = %-8s -> %s\n' COREUTILS  "$(COREUTILS)"  "$(COREUTILS_SRC)"
	@echo "  BOARD=$(BOARD)  MEDIA=$(MEDIA)  LCD=$(if $(LCD),$(LCD),none)"
	@echo "  board targets: KERNEL_TARGET=$(KERNEL_TARGET)  ROOTFS_TARGET=$(ROOTFS_TARGET)"
	@echo "  backend args: KERNEL=$(BUILD_KERNEL) ROOTFS=$(BUILD_ROOTFS) BOOTLOADER=$(BOOTLOADER)"
	@echo "  cfg: $(CFG)"

test:
	@$(REPO_ROOT)/kernel/test/golden.sh

clean:
	rm -rf $(BUILD)/bundles
	@echo "(provider build/ dirs cleaned by their own makefiles)"
