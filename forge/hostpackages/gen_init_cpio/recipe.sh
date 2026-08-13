# hostpackages/gen_init_cpio/recipe.sh — the kernel's newc-cpio writer (declares root-owned /dev
# nodes without root), as a HOST PACKAGE. Not vendored: it's one file in the kernel tree with no
# standalone release, so we pin + fetch just that file (a full-custom build avoids ~140 MB of source).
PKG_NAME=gen_init_cpio
inherit host-cc
# Tracks kernel/mainline's tag; the mirror path + cgit ?h= query below reuse ${PKG_VERSION}.
PKG_VERSION=v6.12.95
PKG_SITE=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/plain/usr
PKG_SITE_MIRROR=https://raw.githubusercontent.com/gregkh/linux/${PKG_VERSION}/usr
PKG_SOURCE=gen_init_cpio.c
PKG_SOURCE_QUERY=?h=${PKG_VERSION}   # cgit needs ?h=<tag>; appended to the URL, not the saved basename
PKG_SHA256=309cab96d2b43133a0758f165fbcf9cc1b671623555ef8dadd9983095c462345
PKG_HOST_BIN=${HOSTTOOLS_DIR}/bin/gen_init_cpio
