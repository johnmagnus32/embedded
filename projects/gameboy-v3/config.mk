# config.mk — the gameboy-v3 PRODUCT selection: which provider implements each layer, plus
# the board. Edit here or override on the CLI to swap the custom from-scratch stack for the
# open-source reference (the payoff of drop-in ABI compatibility). Independent axes:
#   make                                   # full custom stack (the default)
#   make KERNEL=mainline                   # our rootfs on a mainline kernel (bug isolation)
#   make KERNEL=mainline LIBC=musl PACKAGES=busybox BOOTLOADER=uboot   # all-OSS reference
#
# The rootfs is a PACKAGE MODEL: LIBC is the C library (chosen once; everything links it);
# PACKAGES is the additive install set (space-separated). Each package declares its libc
# surface, so LIBC=custom PACKAGES=busybox is rejected up front (busybox needs a complete
# libc — build it on LIBC=musl). See forge/README.md.

# --- provider selection (custom implementation | open-source reference) ------
# NB: trailing whitespace is load-bearing in Make — keep values flush (an aligned comment
# after the value makes the spaces part of it and breaks `ifeq`).
KERNEL     ?= custom
BOOTLOADER ?= custom
LIBC       ?= custom
PACKAGES   ?= coreutils
#   KERNEL     custom -> repo-root kernel/     | mainline -> fetch Linux
#   BOOTLOADER custom -> repo-root bootloader/ | uboot    -> fetch U-Boot
#   LIBC       custom -> repo-root libc/ (gv3libc) | musl -> fetch musl
#   PACKAGES   coreutils -> repo-root coreutils/ | busybox -> fetched OSS (space-separated)
# Version pins live in each recipe (forge/providers|packages/*/recipe.sh).

# --- board + media ------------------------------------------------------------
BOARD      ?= t113-gameboy
MEDIA      ?= nor
#   MEDIA nor -> flash.sh bundle (FEL loop) | sd -> dd-able .img

# Peripherals (LCD, etc.) are NOT axes — a peripheral is invariant board data (DT node +
# driver kconfig) under board/$(BOARD)/, applied by the board, never toggled here.
