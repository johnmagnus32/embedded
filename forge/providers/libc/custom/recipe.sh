# providers/libc/custom/recipe.sh — gv3libc (from-scratch C library), the rootfs libc axis.
PKG_NAME=gv3libc

PKG_FETCH=local
PKG_SOURCE=libc
# gv3libc's build procedure (libc/build.sh) + compile/link contract (libc/libc-profile.sh) stay in
# the source dir; the libc class + cc-profile dispatch on "does that file exist?", not on the name.

# Declaring the toolchain a host dep makes a toolchain-musl bump RIPPLE into gv3libc's taskhash (and,
# since every package depends on libc, transitively into the packages). gv3libc rides the musl
# cross-gcc as a bare compiler (-nostdlib) + its `ar` — see libc/libc-profile.sh.
PKG_HOST_DEPENDS=toolchain-musl

PKG_ARTIFACT=libcstage:       # the link-keyed libc staging dir; skips on an unchanged taskhash

inherit libc
