# packages/coreutils/recipe.sh — our from-scratch userland, as a PACKAGE. One .c per program,
# linked against the selected libc via the compile-c class.
PKG_NAME=coreutils
PKG_FETCH=local
PKG_SOURCE=coreutils
PKG_DEPENDS=libc
inherit compile-c
PKG_INSTALL=/bin
PKG_ARTIFACT=stage:
