# hostpackages/make/recipe.sh — GNU Make >= 4.0, as a HOST PACKAGE (the kernel Makefile requires it;
# the host's make may be older, e.g. 3.82). Built unconditionally; the built make shadows the host's on PATH.
PKG_NAME=make
PKG_CLASS=native
inherit host-autotools
PKG_VERSION=4.4.1
PKG_FETCH=none                      # no single primary source; the tarball is fetched via PKG_SOURCES
PKG_SOURCES="make"
PKG_SRC_make=https://mirrors.kernel.org/gnu/make/make-4.4.1.tar.gz
PKG_SHA_make=dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
PKG_HOST_DEST=${HOSTMAKE_DIR}
PKG_HOST_VERIFY_BIN=make
