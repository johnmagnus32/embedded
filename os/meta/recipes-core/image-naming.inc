# image-naming.inc — the product's artifact TAG scheme, `require`d by the recipes that produce these
# artifacts (rootfs + image), à la Yocto's image.bbclass: naming lives with the image recipes in the
# RECIPE, never in the engine. Derived from the resolved providers (PROVIDER_<x> — the recipe names,
# in the recipe env) + the link mode. A product that ships no initramfs simply doesn't require this —
# the engine (engine.sh) names nothing.
_libctag="$([ "${PROVIDER_libc}" = libc-custom ] && echo "${PROVIDER_libc}-${PROVIDER_cross_cc}" || echo "${PROVIDER_libc}")"
_link="${LINKAGE:-${PKG_LINK:-static}}"
ROOTFS_TAG="${_libctag}-${PROVIDER_init}-$(echo ${PACKAGES} | tr ' ' '+')"
CFG="${PROVIDER_bootloader}-${PROVIDER_kernel}-${ROOTFS_TAG}"
INITRAMFS_IMAGE="initramfs-${ROOTFS_TAG}-${_link}.cpio.gz"
export ROOTFS_TAG CFG INITRAMFS_IMAGE
