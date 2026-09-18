# providers/bootloader/custom/recipe.sh — the from-scratch bootloader (make-c + local source).
PKG_NAME=bootloader
PKG_CLASS=target
PKG_PROVIDES=bootloader

PKG_FETCH=local
PKG_SOURCE=bootloader

inherit make-c
PKG_MAKE_GOALS="all fel"      # default `all`, then `fel` for the FEL-loadable @0x28000 image
PKG_HOST_DEPENDS=cross-cc

# Deploy both eGON images into OUTPUT_DIR: the SD-boot loader + the FEL image the NOR bundle needs.
PKG_DEPLOY="build/bootloader.egon.bin:bootloader.bin build/bootloader-fel-0x28000.egon.bin:fel-loader.bin"
