# image/Makefile — builds the gameboy-v3 OS image. THIS dir is the engine's PRODUCT_DIR: the layer
# (conf/local.conf + conf/machine/ + recipes-*/) and the build/ workspace live here. Including the shared
# engine gives the `image` target (default goal), every recipe as a target by name, and `clean`.
#
# Every build names a MACHINE — no magical default (a bare `make` fails loud with the list):
#   MACHINE=t113 make           # the shippable hw image -> build/t113/
#   MACHINE=qemu make rootfs            # build one recipe by name, on QEMU (-M virt)
#   MACHINE=t113 make clean     # -> build/t113
# Usually driven from the product Makefile one level up (its image/canvas/flash targets, or
# `make -C image <recipe>`); this is also the place to build individual recipes directly.
PRODUCT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT   := $(abspath $(PRODUCT_DIR)/../../..)
include $(REPO_ROOT)/os/engine/engine.mk
