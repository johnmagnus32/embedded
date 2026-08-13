# hostpackages/toolchain-glibc/recipe.sh — the glibc arm cross toolchain (prebuilt Bootlin tarball),
# as a HOST PACKAGE. Builds U-Boot + the kernel, which don't link a target libc.
PKG_NAME=toolchain-glibc
inherit host-tarball-bin
PKG_VERSION=2021.11-1
PKG_SITE=https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs
PKG_SOURCE=armv7-eabihf--glibc--stable-2021.11-1.tar.bz2
PKG_SHA256=6d10f356811429f1bddc23a174932c35127ab6c6f3b738b768f0c29c3bf92f10
PKG_HOST_DEST=${TOOLCHAIN_DIR}
PKG_HOST_CC_PREFIX=${CROSS_COMPILE}
