# providers/bootloader/custom/recipe.sh — the from-scratch bootloader (make-c + local source).
PKG_NAME=gv3boot

PKG_FETCH=local
PKG_SOURCE=bootloader

inherit make-c
PKG_MAKE_GOALS="all fel"      # default `all`, then `fel` for the FEL-loadable @0x28000 image
PKG_HOST_DEPENDS=toolchain-glibc

# SD-boot eGON; the FEL image is a secondary the NOR bundle also needs.
PKG_ARTIFACT=src:build/gv3boot.egon.bin
PKG_ARTIFACT_FEL=src:build/gv3boot-fel-0x28000.egon.bin
