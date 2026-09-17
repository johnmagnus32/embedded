# Makefile — gameboy-v3 PRODUCT entry point (thin). Every build names a CONFIG (configs/<name>.conf);
# there is NO default — a bare `make` fails loud with the list. Each config gets its own build/<name>/
# cache (shared build/downloads). The shared engine (os/) turns a config into an image. To switch a
# provider (kernel/libc/…), edit its PREFERRED_PROVIDER_virtual/<slot> in the config; PACKAGES / MEDIA /
# LINKAGE are plain knobs, overridable as ENV vars:
#
#   CONFIG=t113 make image        # the shippable hw build (configs/t113.conf) -> build/t113/
#   CONFIG=virt-musl make rootfs  # a QEMU virt test rootfs (configs/virt-musl.conf) -> build/virt-musl/
#   NB: override knobs via the ENVIRONMENT (`MEDIA=sd CONFIG=t113 make image`), NOT `make MEDIA=…` — a
#       command-line make-var doesn't reach the engine.
#   make canvas          # alias: CONFIG=t113 make image
#   CONFIG=t113 make <recipe>   # build one recipe by name: linux, musl, busybox, toolchain-gcc, rootfs, …
#   make flash           # build the t113 NOR bundle + flash + FEL-boot on the rig
#   make test            # kernel golden QEMU tests
#   make clean           # CONFIG=<name> make clean -> build/<name>; bare make clean -> nuke build/

PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include $(REPO_ROOT)/os/engine/engine.mk

# --- product-specific targets (deploy/verify on OUR bench, so not in the generic engine) ---------
.PHONY: flash test canvas
# canvas: alias for the shippable hw image — CONFIG=t113 (configs/t113.conf already selects busybox +
# the product-local `console` package). (`make console` builds just the console package recipe.)
canvas:
	@CONFIG=t113 $(MAKE) image

# flash: build the t113 NOR bundle + flash + FEL-boot it on the rig. NOR-only (an SD image is a manual
# dd of build/t113/output/*-sd.img), so force CONFIG=t113 + MEDIA=nor and hand flash.sh the bundle path.
flash:
	@CONFIG=t113 MEDIA=nor $(MAKE) image
	@$(REPO_ROOT)/tools/flash.sh $(PRODUCT_DIR)/build/t113/bundle nor

test:
	@$(REPO_ROOT)/kernel/test/golden.sh
