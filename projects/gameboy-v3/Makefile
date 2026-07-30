# Makefile — gameboy-v3 PRODUCT entry point (thin).
#
# A product is mostly configuration: config.mk declares which provider implements
# each layer + the board; the shared engine (forge/) turns that into an image.
#
#   make                 # build per config.mk (default: full custom stack, MEDIA=nor)
#   make KERNEL=mainline LIBC=musl COREUTILS=busybox BOOTLOADER=uboot   # all-OSS reference
#   make KERNEL=mainline # our rootfs on a mainline kernel (bug isolation)
#   make flash           # build a NOR bundle + flash + FEL-boot on the rig
#   make print-config    # show the resolved selection (no build)
#   make test            # kernel golden QEMU tests
#   make clean

PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../..)

include config.mk
include $(REPO_ROOT)/forge/rules.mk
