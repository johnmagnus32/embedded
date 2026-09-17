# Makefile — gameboy-v3 PRODUCT entry point (thin). The selection lives in local.conf (the default
# config, key=value data) — or configs/<name>.conf, chosen with CONFIG=<name>, each getting its own
# build/<name>/ cache (shared build/downloads). The shared engine (os/) turns a config into an image.
# To switch a provider (kernel/libc/…), edit its PREFERRED_PROVIDER_virtual/<slot> in the config;
# PACKAGES / MEDIA / LINKAGE are plain knobs, overridable as ENV vars:
#
#   make                          # build per local.conf (default: linux + musl + busybox + console, nor)
#   NB: override via the ENVIRONMENT (`PACKAGES=busybox make …`), NOT `make PACKAGES=…` — a command-line
#       make-var doesn't reach the engine. local.conf holds the defaults.
#   CONFIG=virt-musl make rootfs  # build a named config (configs/virt-musl.conf) in build/virt-musl/
#   MEDIA=sd make image           # an SD .img instead of the NOR bundle
#   make canvas          # the canvas console flavor (busybox + the console package)
#   make <recipe>        # build one node by its recipe name: linux, musl, busybox, toolchain-gcc, rootfs, …
#   make flash           # build a NOR bundle + flash + FEL-boot on the rig
#   make test            # kernel golden QEMU tests
#   make clean

PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)
include $(REPO_ROOT)/os/engine/engine.mk

# --- product-specific targets (deploy/verify on OUR bench, so not in the generic engine) ---------
.PHONY: flash test canvas
# canvas: the console flavor — the product-local `console` package alongside busybox (kernel/libc come
# from local.conf). A named selection so you don't retype PACKAGES; override MEDIA=sd for an SD image.
# (`make console` builds just the console package node.)
canvas:
	@PACKAGES="busybox console" $(MAKE) image

# flash: build the NOR bundle + flash + FEL-boot it on the rig. This product is NOR-only (an SD image
# is a manual dd of build/output/*-sd.img), so force MEDIA=nor and hand flash.sh the fixed bundle path.
flash:
	@MEDIA=nor $(MAKE) image
	@$(REPO_ROOT)/tools/flash.sh $(PRODUCT_DIR)/build/bundle nor

test:
	@$(REPO_ROOT)/kernel/test/golden.sh
