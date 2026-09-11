# hostpackages/toolchain-musl/recipe.sh — the musl arm cross toolchain (prebuilt Bootlin tarball),
# as a HOST PACKAGE. Builds the rootfs (musl-static is ~34% smaller than glibc-static).
PKG_NAME=toolchain-musl
inherit host-tarball-bin
PKG_VERSION=2021.11-1
PKG_SITE=https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs
PKG_SOURCE=armv7-eabihf--musl--stable-2021.11-1.tar.bz2
PKG_SHA256=767c99155f74d5620cfd59d0224df2f82dec7ce58be24d702081dca9793408a9
PKG_HOST_DEST=${ROOTFS_TOOLCHAIN_DIR}
PKG_HOST_CC_PREFIX=${ROOTFS_CROSS_COMPILE}
