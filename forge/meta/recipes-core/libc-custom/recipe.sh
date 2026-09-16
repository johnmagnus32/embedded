# providers/libc/custom/recipe.sh — libc (from-scratch C library), the rootfs libc axis.
PKG_NAME=libc
PKG_CLASS=target
PKG_PROVIDES=virtual/libc
PKG_ALIAS=custom

PKG_FETCH=local
PKG_SOURCE=libc
# libc's build procedure (libc/build.sh) + compile/link contract (libc/libc-profile.sh) stay in
# the source dir; the libc class + cc-profile dispatch on "does that file exist?", not on the name.

# Host toolchain = the resolved LIBC_TC (from forge.conf): the toolchain the libc is BUILT WITH — the
# STAGE-1 toolchain-gcc-initial for TOOLCHAIN=gcc (libc/build-sysroot.sh compiles the libc into a
# conforming sysroot with it; the STAGE-2 toolchain-gcc then builds --with-sysroot=that), or
# toolchain-custom for TOOLCHAIN=custom (our own cc builds the libc directly, no stage split).
# NB the gcc path DIVERGES the two toolchains: the libc builds with
# LIBC_TC (initial) but packages compile with ROOTFS_TC (final) — the final PKG_DEPENDS on this libc.
# Declaring LIBC_TC makes a stage-1 bump RIPPLE into libc's taskhash (and, via PKG_DEPENDS=libc, into
# the final gcc + every package). ${LIBC_TC} is bash-expanded when run-recipe.sh sources this recipe
# (forge.conf already loaded); engine.mk's Make edge reads the same LIBC_TC.
PKG_HOST_DEPENDS=${LIBC_TC}

PKG_ARTIFACT=libcstage:       # the link-keyed libc staging dir; skips on an unchanged taskhash

inherit libc
