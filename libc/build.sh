# libc/build.sh — gv3libc's BUILD PROCEDURE (how to BUILD this libc).
#
# The libc-provider analogue of the kernel/u-boot providers' build.sh: the engine
# (forge/core/classes/libc.sh) stays libc-agnostic and runs this when the selected
# libc is this from-source provider. All gv3libc build knowledge — compile crt0 +
# every unit, archive (static) or shared-lib + the ld-gv3.so.1 dynamic linker
# (dynamic), and stage the kernel UAPI snapshot — lives HERE, with the libc. A
# prebuilt libc (musl) ships no build.sh, so the engine no-ops for it (its sysroot is
# already complete — the standard Buildroot non-custom path).
#
# Sourced (not exec'd) by the libc class (classes/libc.sh), which runs in run-recipe.sh's shell
# (build env already loaded) AFTER sourcing the libc compile profile (cc-profile.sh ->
# libc-profile.sh), so PKG_CC/PKG_CFLAGS and the LIBC_STAGE_DIR/STAGE_INC paths are already set.
# Contract (env in):
#   PKG_CC PKG_CFLAGS   the compile profile (from libc-profile.sh via cc-profile.sh)
#   PKG_LINK            static | dynamic
#   LIBC_STAGE_DIR      output dir (crt0.S.o / libc.a / libc.so / ld-gv3.so.1)
#   STAGE_INC           where to stage the kernel UAPI headers
#   ROOTFS_TARGET       t113 | virt (the arch/board target, passed to the ld/ sub-make as BOARD=)
#   REPO_ROOT ROOTFS_CROSS_COMPILE   (from the build env)

LIBC_PROVIDER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_UAPI_DIR="${REPO_ROOT}/kernel/include/uapi"
: "${LIBC_STAGE_DIR:?libc/build.sh: LIBC_STAGE_DIR unset}"; : "${STAGE_INC:?libc/build.sh: STAGE_INC unset}"
: "${PKG_CC:?libc/build.sh: PKG_CC unset (source cc-profile first)}"
PKG_LINK="${PKG_LINK:-static}"

mkdir -p "${LIBC_STAGE_DIR}" "${STAGE_INC}"

# 1. stage the kernel UAPI snapshot (syscall numbers + ABI structs) the libc + its
#    consumers compile against — the `make headers_install` model.
for h in gv3_syscalls.h gv3_abi.h; do
  cp -f "${KERNEL_UAPI_DIR}/${h}" "${STAGE_INC}/${h}"
done

# 2. crt0 (assembly _start) — always linked into the executable, never the library.
"${PKG_CC}" ${PKG_CFLAGS} $([ "${PKG_LINK}" = dynamic ] && echo -fPIC) \
  -c "${LIBC_PROVIDER_DIR}/src/crt/crt0.S" -o "${LIBC_STAGE_DIR}/crt0.S.o"

# 3. compile every libc unit (auto-discovered) into objects.
OBJS=()
for c in "${LIBC_PROVIDER_DIR}"/src/*.c; do
  o="${LIBC_STAGE_DIR}/$(basename "${c}").o"
  "${PKG_CC}" ${PKG_CFLAGS} $([ "${PKG_LINK}" = dynamic ] && echo -fPIC) -c "${c}" -o "${o}"
  OBJS+=("${o}")
done

# 4. archive (static) and/or shared lib + the dynamic linker (dynamic).
if [ "${PKG_LINK}" = dynamic ]; then
  "${PKG_CC}" ${PKG_CFLAGS} -fPIC -shared -nostdlib -Wl,--build-id=none \
    -Wl,-soname,libc.so "${OBJS[@]}" -o "${LIBC_STAGE_DIR}/libc.so"
  # the from-scratch dynamic linker rides WITH the libc (ld/ Makefile), built into
  # the libc staging dir.
  make -C "${LIBC_PROVIDER_DIR}/ld" BUILD="${LIBC_STAGE_DIR}/ld-build" BOARD="${ROOTFS_TARGET}" >/dev/null 2>&1
  cp -f "${LIBC_STAGE_DIR}/ld-build/ld-gv3.so.1" "${LIBC_STAGE_DIR}/ld-gv3.so.1"
  echo "  [libc] gv3libc.so + ld-gv3.so.1 -> ${LIBC_STAGE_DIR}"
  # PRODUCE-ONLY: libc.so + ld-gv3.so.1 land in LIBC_STAGE_DIR, a self-contained artifact.
  # INSTALLING them into the image's /lib is the ROOTFS assembler's job (§3f) — a producer
  # never writes $STAGE. The assembler reads these from LIBC_STAGE_DIR for a dynamic build.
else
  "${ROOTFS_CROSS_COMPILE}ar" rcs "${LIBC_STAGE_DIR}/libc.a" "${OBJS[@]}"
  echo "  [libc] gv3libc.a -> ${LIBC_STAGE_DIR}"
fi
