# Makefile — gameboy-v3 PRODUCT entry point (thin). The selection lives in local.conf (bash, read by
# the os scripts); the shared engine (os/) turns it into an image. Override knobs as env vars:
#
#   make                 # build per local.conf (default: mainline + musl + busybox + console, MEDIA=nor)
#   NB: override via the ENVIRONMENT (`LIBC=custom make …`), NOT `make LIBC=custom` — a command-line
#       make-var doesn't reach the resolver. local.conf holds the defaults.
#   KERNEL=mainline LIBC=musl PACKAGES=busybox BOOTLOADER=uboot make image   # all-OSS reference
#   KERNEL=mainline make image                                               # our rootfs on a mainline kernel
#   make canvas          # the canvas console flavor (mainline + musl + the console package)
#   make <recipe>        # build one node by its recipe name: linux, musl, busybox, toolchain-gcc, rootfs, …
#   make flash           # build a NOR bundle + flash + FEL-boot on the rig
#   make test            # kernel golden QEMU tests
#   make clean

PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include $(REPO_ROOT)/os/engine/engine.mk

# --- product-specific targets (deploy/verify on OUR bench, so not in the generic engine) ---------
.PHONY: flash test canvas
# canvas: the console flavor — mainline + musl + the product-local `console` package alongside busybox.
# A named selection so you don't retype the knobs; override MEDIA=sd for an SD image. (`make console`
# builds just the console package node.)
canvas:
	@KERNEL=mainline LIBC=musl PACKAGES="busybox console" $(MAKE) image

# flash: BUNDLE + MEDIA are resolved by the engine (os-env), not Make — pull them for the tool.
flash: image
	@eval "$$(PRODUCT_DIR=$(PRODUCT_DIR) $(OS_DIR)/os-env.sh print BUNDLE MEDIA)"; \
	  if [ "$$MEDIA" != nor ]; then \
	    echo "make flash needs MEDIA=nor (SD is a manual dd of build/output/*-sd.img)"; exit 1; fi; \
	  $(REPO_ROOT)/tools/flash.sh "$$BUNDLE" nor

test:
	@$(REPO_ROOT)/kernel/test/golden.sh
