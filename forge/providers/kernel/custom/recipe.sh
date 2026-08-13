# providers/kernel/custom/recipe.sh — the from-scratch kernel (make-c + local source). Board facts
# (defconfig/DTB/console) live in board/<board>/board.conf, not here.
PKG_NAME=gv3kernel

PKG_FETCH=local
PKG_SOURCE=kernel

inherit make-c
PKG_HOST_DEPENDS=toolchain-glibc

# ${KERNEL_TARGET} is expanded by the composer (which knows the board target).
PKG_ARTIFACT=src:build/${KERNEL_TARGET}/gv3kernel.bin
