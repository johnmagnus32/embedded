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

# --- test: the harness GLUE (the ONLY os-aware piece) -------------------------
# The three harnesses know NOTHING about os/: kernel/test/golden.sh,
# libc/test/dynamic.sh, libc/test/on-target/run.sh each take their inputs as env
# vars (toolchain prefix, kernel path, gen_init_cpio, rootfs images). This target
# is the SOLE place that builds engine artifacts and maps their paths onto those
# env-var contracts (like a Yocto ptest wrapper, or `make -C selftests CC=...`).
#
# Artifacts per config (build/<config>/): toolchain prefix, gen_init_cpio host
# tool, and the packed initramfs (named initramfs-<rootfstag>-<link>.cpio.gz).
VIRT_MUSL_DIR         := $(PRODUCT_DIR)/build/virt-musl
VIRT_CUSTOM_DIR       := $(PRODUCT_DIR)/build/virt-custom
FORGE_PREFIX          := arm-forge-linux-gnueabihf-
MUSL_CROSS            := $(VIRT_MUSL_DIR)/toolchain-gcc/bin/$(FORGE_PREFIX)
CUSTOM_CROSS          := $(VIRT_CUSTOM_DIR)/toolchain-gcc/bin/$(FORGE_PREFIX)
MUSL_GENCPIO          := $(VIRT_MUSL_DIR)/hosttools/bin/gen_init_cpio
CUSTOM_GENCPIO        := $(VIRT_CUSTOM_DIR)/hosttools/bin/gen_init_cpio
# virt-musl (musl + init-shell + busybox): rootfs tag = musl-init-shell-busybox
MUSL_STATIC_INITRD    := $(VIRT_MUSL_DIR)/output/initramfs-musl-init-shell-busybox-static.cpio.gz
MUSL_DYNAMIC_INITRD   := $(VIRT_MUSL_DIR)/output/initramfs-musl-init-shell-busybox-dynamic.cpio.gz
# virt-custom (libc-custom + init-shell + coreutils): our-ld.so dynamic rootfs
CUSTOM_DYNAMIC_INITRD := $(VIRT_CUSTOM_DIR)/output/initramfs-libc-custom-toolchain-gcc-init-shell-coreutils-dynamic.cpio.gz
#
# run.sh + dynamic.sh boot a MAINLINE virt zImage. The engine builds one — `CONFIG=virt-musl make
# linux` (multi_v7_defconfig; -M virt is self-describing, so no DTB) deploys it here. Override to
# boot a different kernel: `VIRT_REFKERNEL=/path/to/virt-zImage make test`.
VIRT_REFKERNEL ?= $(VIRT_MUSL_DIR)/output/zImage

test:
	@echo "=== golden.sh: custom-kernel boot cases (fully wired) ==="
	@CONFIG=virt-musl $(MAKE) rootfs
	@CONFIG=virt-musl LINKAGE=dynamic $(MAKE) rootfs
	@CROSS_COMPILE=$(MUSL_CROSS) GEN_INIT_CPIO=$(MUSL_GENCPIO) \
	    BUSYBOX_INITRD=$(MUSL_STATIC_INITRD) DYNAMIC_INITRD=$(MUSL_DYNAMIC_INITRD) \
	    $(REPO_ROOT)/kernel/test/golden.sh
	@echo
	@echo "=== building the mainline virt reference kernel (run.sh + dynamic.sh boot it) ==="
	@CONFIG=virt-musl $(MAKE) linux
	@echo "=== run.sh: libc syscall-wrapper units ==="
	@CONFIG=virt-custom $(MAKE) rootfs
	@CROSS_COMPILE=$(CUSTOM_CROSS) GEN_INIT_CPIO=$(CUSTOM_GENCPIO) KERNEL="$(VIRT_REFKERNEL)" \
	    $(REPO_ROOT)/libc/test/on-target/run.sh
	@echo "=== dynamic.sh: dynamic-linking boot test ==="
	@CONFIG=virt-custom LINKAGE=dynamic $(MAKE) rootfs
	@CROSS_COMPILE=$(MUSL_CROSS) GEN_INIT_CPIO=$(MUSL_GENCPIO) \
	    REFKERNEL="$(VIRT_REFKERNEL)" DYNAMIC_INITRD=$(CUSTOM_DYNAMIC_INITRD) \
	    $(REPO_ROOT)/libc/test/dynamic.sh
