# toolchain-gcc-sources.inc — SHARED DATA for our from-source arm cross toolchain (Yocto's
# gcc-${PV}.inc analogue). `require`d by BOTH stage recipes (toolchain-gcc-initial + toolchain-gcc)
# so the pins + triple + arch tuning live ONCE: the two stages MUST build byte-identical gcc/binutils
# (stage-2's libgcc links a libc that stage-1's gcc compiled — a version skew is an ABI landmine).
# This is DATA, so it lives in a .inc, not the host-toolchain-gcc CLASS (which holds shared LOGIC).
# require records it for compute_recipe_stamp, so a bump here re-hashes both stages → libc → packages.
# To bump a component: change its PKG_SRC_<name> URL + PKG_SHA_<name> (content is pinned by SHA256;
# the URL is just availability).

# (the cross triple / PKG_HOST_CC_PREFIX lives in each stage recipe now — resolve.mk reads it via
#  _field on the selected virtual/cross-cc provider, so it must be a recipe field, not a shared .inc one.)
# arch baseline (T113 Cortex-A7, hard-float VFP). Board-independent — per-board -mgeneral-regs-only is
# a package COMPILE flag (ROOTFS_ARCH_FLAGS), not a toolchain-build one, so one toolchain serves all boards.
PKG_TC_CPU=cortex-a7
PKG_TC_FPU=neon-vfpv4

PKG_SOURCES="binutils gcc gmp mpfr mpc isl"
PKG_SRC_binutils=https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz
PKG_SHA_binutils=f6e4d41fd5fc778b06b7891457b3620da5ecea1006c6a4a41ae998109f85a800
PKG_SRC_gcc=https://ftp.gnu.org/gnu/gcc/gcc-13.3.0/gcc-13.3.0.tar.xz
PKG_SHA_gcc=0845e9621c9543a13f484e94584a49ffc0129970e9914624235fc1d061a0c083
PKG_SRC_gmp=https://gcc.gnu.org/pub/gcc/infrastructure/gmp-6.2.1.tar.bz2
PKG_SHA_gmp=eae9326beb4158c386e39a356818031bd28f3124cf915f8c5b1dc4c7a36b4d7c
PKG_SRC_mpfr=https://gcc.gnu.org/pub/gcc/infrastructure/mpfr-4.1.0.tar.bz2
PKG_SHA_mpfr=feced2d430dd5a97805fa289fed3fc8ff2b094c02d05287fd6133e7f1f0ec926
PKG_SRC_mpc=https://gcc.gnu.org/pub/gcc/infrastructure/mpc-1.2.1.tar.gz
PKG_SHA_mpc=17503d2c395dfcf106b622dc142683c1199431d095367c6aacba6eec30340459
PKG_SRC_isl=https://gcc.gnu.org/pub/gcc/infrastructure/isl-0.24.tar.bz2
PKG_SHA_isl=fcf78dd9656c10eb8cf9fbd5f59a0b6b01386205fe1934b3b287a0a1898145c0
