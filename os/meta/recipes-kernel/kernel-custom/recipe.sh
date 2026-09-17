# providers/kernel/custom/recipe.sh — the from-scratch kernel (make-c + local source). Board facts
# (defconfig/DTB/console) live in board/<board>/board.conf, not here.
PKG_NAME=kernel
PKG_CLASS=target
PKG_PROVIDES=virtual/kernel

PKG_FETCH=local
PKG_SOURCE=kernel

inherit make-c
PKG_HOST_DEPENDS=virtual/cross-cc

# Deploy the zImage-shaped kernel into OUTPUT_DIR under the convention image reads.
PKG_DEPLOY="build/${KERNEL_TARGET}/kernel.bin:zImage"
