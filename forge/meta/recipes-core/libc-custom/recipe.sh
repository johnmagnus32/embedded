# recipes-core/libc-custom/recipe.sh — libc (from-scratch C library), a rootfs libc (virtual/libc).
PKG_NAME=libc
PKG_CLASS=target
PKG_PROVIDES=virtual/libc

PKG_FETCH=local
PKG_SOURCE=libc
# libc's build procedure (libc/build.sh) + compile/link contract (libc/libc-profile.sh) stay in
# the source dir; the libc class + cc-profile dispatch on "does that file exist?", not on the name.

# Host toolchain = virtual/cross-cc-initial: the toolchain the libc is BUILT WITH — the STAGE-1
# toolchain-gcc-initial for TOOLCHAIN=gcc (libc/build-sysroot.sh compiles the libc into a conforming
# sysroot with it; the STAGE-2 toolchain-gcc then builds --with-sysroot=that), or toolchain-custom for
# TOOLCHAIN=custom (our own cc builds the libc directly, no stage split). NB the gcc path DIVERGES the
# two toolchains: the libc builds with the stage-1 compiler but packages compile with virtual/cross-cc
# (the final), which PKG_DEPENDS on this libc. The virtual edge makes a stage-1 bump RIPPLE into libc's
# taskhash (and, via PKG_DEPENDS=virtual/libc on packages, into the final gcc + every package).
PKG_HOST_DEPENDS=virtual/cross-cc-initial

PKG_ARTIFACT=libcstage:       # the link-keyed libc staging dir; skips on an unchanged taskhash

inherit libc
