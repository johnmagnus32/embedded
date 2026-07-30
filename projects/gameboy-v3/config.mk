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
# NOTE (Phase 0): the resolver + Makefile here still drive the existing
# scripts/build.sh; providers still live in-project (nothing has moved yet).
# Later phases graduate the providers to embedded/ and the engine to forge/.

# --- provider selection (custom implementation | open-source reference) ------
# NB: trailing whitespace matters in Make — keep values flush (no aligned comments
# after the value, or the spaces become part of it and break `ifeq` comparisons).
KERNEL     ?= custom
BOOTLOADER ?= custom
LIBC       ?= gv3
COREUTILS  ?= gv3
#   KERNEL     custom -> our kernel/       | mainline -> fetch Linux
#   BOOTLOADER custom -> our bootloader/   | uboot    -> fetch U-Boot
#   LIBC       gv3    -> our rootfs/libc   | musl     -> fetch musl
#   COREUTILS  gv3    -> our rootfs/bin    | busybox  -> fetch BusyBox

# --- board + media ------------------------------------------------------------
BOARD      ?= t113-gameboy
MEDIA      ?= nor
#   MEDIA nor -> flash.sh bundle (FEL loop) | sd -> dd-able .img

# --- optional peripherals -----------------------------------------------------
LCD        ?=
#   LCD empty | ili9341 (mainline DRM tiny driver on SPI1)
