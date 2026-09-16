# hostpackages/libconfuse/recipe.sh — genimage's one dependency, as a HOST PACKAGE. Static-only,
# into the shared HOSTTOOLS_DIR prefix so genimage's configure finds its .a + pkgconfig + header.
PKG_NAME=libconfuse
PKG_CLASS=native
inherit host-autotools
PKG_VERSION=3.3
PKG_FETCH=none                      # no single primary source; the tarball is fetched via PKG_SOURCES
PKG_SOURCES="confuse"
PKG_SRC_confuse=https://github.com/libconfuse/libconfuse/releases/download/v3.3/confuse-3.3.tar.gz
PKG_SHA_confuse=3a59ded20bc652eaa8e6261ab46f7e483bc13dad79263c15af42ecbb329707b8
PKG_HOST_DEST=${HOSTTOOLS_DIR}
PKG_HOST_CONFIGURE_FLAGS="--disable-shared --enable-static --disable-examples"
