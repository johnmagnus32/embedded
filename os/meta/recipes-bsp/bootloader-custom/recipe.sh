# providers/bootloader/custom/recipe.sh — the from-scratch bootloader (make-c + local source).
PKG_NAME=bootloader
PKG_CLASS=target
PKG_PROVIDES=virtual/bootloader

PKG_FETCH=local
PKG_SOURCE=bootloader

inherit make-c
PKG_MAKE_GOALS="all fel"      # default `all`, then `fel` for the FEL-loadable @0x28000 image
PKG_HOST_DEPENDS=virtual/cross-cc

# SD-boot eGON; the FEL image is a secondary the NOR bundle also needs.
PKG_ARTIFACT=src:build/bootloader.egon.bin
PKG_ARTIFACT_FEL=src:build/bootloader-fel-0x28000.egon.bin
