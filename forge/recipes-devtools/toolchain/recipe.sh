# recipes-devtools/toolchain/recipe.sh — the `toolchain` AGGREGATE (Yocto packagegroup analogue): a
# recipe whose only job is to pull the host tools this selection uses, so `make toolchain` provisions the
# cross toolchain (virtual/cross-cc) + cpio writer up front for humans and the test harnesses. It builds
# nothing itself (PKG_CLASS=aggregate => no artifact, never cached); component builds don't depend on it —
# they pull the toolchain via virtual/cross-cc themselves.
PKG_NAME=toolchain
PKG_CLASS=aggregate
PKG_HOST_DEPENDS="virtual/cross-cc gen_init_cpio"
PKG_FETCH=none

do_build() {
  printf '\033[1;32m[toolchain] DONE\033[0m — cross toolchain (%s) + gen_init_cpio provisioned\n' "${ROOTFS_TC:-?}"
}
do_install() { :; }
