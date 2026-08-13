# hostpackages/genimage/recipe.sh — the declarative SD-image assembler (Buildroot's tool), built
# from source. LAZY: only image.sh's MEDIA=sd path declares it. PKG_HOST_DEPENDS=libconfuse is the
# one host->host edge in the tree (built first, into the shared prefix genimage's configure reads).
PKG_NAME=genimage
inherit host-autotools
PKG_VERSION=18
PKG_SITE=https://github.com/pengutronix/genimage/releases/download/v18
PKG_SOURCE=genimage-18.tar.xz
PKG_SHA256=ebc3f886c4d80064dd6c6d5e3c2e98e5a670078264108ce2f89ada8a2e13fedd
PKG_HOST_DEPENDS=libconfuse
PKG_HOST_DEST=${HOSTTOOLS_DIR}
PKG_HOST_VERIFY_BIN=genimage
# quoted: host-autotools word-splits this into `env` args before ./configure, so it finds libconfuse.
PKG_HOST_CONFIGURE_ENV="PKG_CONFIG_PATH=${HOSTTOOLS_DIR}/lib/pkgconfig CPPFLAGS=-I${HOSTTOOLS_DIR}/include LDFLAGS=-L${HOSTTOOLS_DIR}/lib"
