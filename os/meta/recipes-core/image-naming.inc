# image-naming.inc — the product's artifact TAG scheme, `require`d by the recipes that produce these
# artifacts (rootfs + image), à la Yocto's image.bbclass: naming lives with the image recipes in the
# LAYER, never in the engine. Derived from the selection knobs (in the recipe env, from local.conf) +
# the link mode. A product that ships no initramfs simply doesn't require this — the engine (engine.sh)
# names nothing.
_libctag="$([ "${LIBC}" = custom ] && echo "${LIBC}-${TOOLCHAIN}" || echo "${LIBC}")"
_link="${LINKAGE:-${PKG_LINK:-static}}"
ROOTFS_TAG="${_libctag}-${INIT}-$(echo ${PACKAGES} | tr ' ' '+')"
CFG="${BOOTLOADER}-${KERNEL}-${ROOTFS_TAG}"
INITRAMFS_IMAGE="initramfs-${ROOTFS_TAG}-${_link}.cpio.gz"
export ROOTFS_TAG CFG INITRAMFS_IMAGE
