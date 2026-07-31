# config.mk — the gameboy-v3 PRODUCT selection.
#
# A product is mostly configuration: which provider implements each layer, plus
# the board. This is the file you edit (or override on the `make` command line)
# to choose the custom from-scratch stack vs. the open-source reference — the
# payoff of drop-in ABI compatibility.
#
# Providers are INDEPENDENT axes (see forge/providers.mk). Override any on the CLI:
#   make                                   # full custom stack (the default)
#   make KERNEL=mainline                   # our rootfs on a mainline kernel (bug isolation)
#   make KERNEL=mainline LIBC=musl COREUTILS=busybox BOOTLOADER=uboot   # all-OSS reference
#
# The Makefile here is a thin `include ../../forge/rules.mk`; the shared engine
# (forge/) + the generic providers (repo-root kernel/ bootloader/ libc/ coreutils/)
# + the build backends (forge/backends/) all live outside this product. This file
# + versions.env + board/ are the only per-product inputs. See forge/README.md.

# --- provider selection (custom implementation | open-source reference) ------
# NB: trailing whitespace matters in Make — keep values flush (no aligned comments
# after the value, or the spaces become part of it and break `ifeq` comparisons).
KERNEL     ?= custom
BOOTLOADER ?= custom
LIBC       ?= custom
COREUTILS  ?= custom
#   KERNEL     custom -> repo-root kernel/     | mainline -> fetch Linux
#   BOOTLOADER custom -> repo-root bootloader/ | uboot    -> fetch U-Boot
#   LIBC       custom -> repo-root libc/       | musl     -> fetch musl
#   COREUTILS  custom -> repo-root coreutils/  | busybox  -> fetch BusyBox
# OSS component versions: versions.env. Board facts: board/$(BOARD)/.

# --- board + media ------------------------------------------------------------
BOARD      ?= t113-gameboy
MEDIA      ?= nor
#   MEDIA nor -> flash.sh bundle (FEL loop) | sd -> dd-able .img

# --- optional peripherals -----------------------------------------------------
LCD        ?=
#   LCD empty | ili9341 (mainline DRM tiny driver on SPI1)
