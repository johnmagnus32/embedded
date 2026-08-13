# steps/rootfs/recipe.sh — the rootfs engine step: pack the initramfs. PACKAGES are their own
# graph nodes (built + staged first); this step only assembles + packs them. (Yocto's do_rootfs.)
PKG_NAME=rootfs

PKG_DEPENDS=${PACKAGES}
PKG_FETCH=none
PKG_HOST_DEPENDS=gen_init_cpio          # the newc-cpio writer

# init.sh is renamed to /init (the portable PID-1); every other overlay file lands at its rel path.
_rootfs_overlay_merge() {
  local stage="$1" overlay="$2"
  [ -d "$overlay" ] || return 0
  local f dst
  for f in $(cd "$overlay" && find . -type f | sed 's|^\./||'); do
    dst="$stage/$f"
    [ "$f" = "init.sh" ] && dst="$stage/init"
    mkdir -p "$(dirname "$dst")"
    install -m 0755 "$overlay/$f" "$dst"
  done
}

_rootfs_pack() {
  : "${STAGE:?}"; : "${DEVTABLE_DEFAULT:?}"; : "${OUT_CPIO:?}"; : "${HOSTTOOLS_DIR:?}"
  # gen_init_cpio is this step's host dep (PKG_HOST_DEPENDS); resolve its provisioned path here
  # (host tools aren't on PATH — invoked by full path, like the image step does with genimage).
  local GEN_INIT_CPIO="${HOSTTOOLS_DIR}/bin/gen_init_cpio"
  [ -x "${GEN_INIT_CPIO}" ] || die "rootfs: gen_init_cpio not provisioned at ${GEN_INIT_CPIO}"
  _rootfs_overlay_merge "${STAGE}" "${OVERLAY_DIR:-}"
  local listing="${OUT_CPIO%.cpio.gz}.list"
  {
    # Device table: engine default then optional product augment — a later line wins on a
    # duplicate path (Buildroot semantics). It declares the /dev nodes + the proc/sys/dev
    # mountpoints that the tree-walk below prunes (they're supplied here, not from STAGE).
    for t in "${DEVTABLE_DEFAULT}" "${DEVTABLE:-}"; do
      [ -n "$t" ] && [ -f "$t" ] && grep -v '^[[:space:]]*#' "$t" | grep -v '^[[:space:]]*$'
    done
    cd "${STAGE}"
    find . -mindepth 1 \( -path './proc' -o -path './sys' -o -path './dev' \) -prune -o -print \
    | while read -r p; do
        rel="${p#.}"
        if [ -L "$p" ]; then
          printf 'slink %s %s 0777 0 0\n' "$rel" "$(readlink "$p")"
        elif [ -d "$p" ]; then
          # `stat -c '%a'` yields OCTAL digits (755) — emit via %s with a leading 0.
          # NEVER printf %o: it reads "755" as decimal and corrupts every mode.
          printf 'dir %s 0%s 0 0\n' "$rel" "$(stat -c '%a' "$p")"
        elif [ -f "$p" ]; then
          printf 'file %s %s 0%s 0 0\n' "$rel" "${STAGE}$rel" "$(stat -c '%a' "$p")"
        fi
      done
  } > "${listing}"
  mkdir -p "$(dirname "${OUT_CPIO}")"
  "${GEN_INIT_CPIO}" "${listing}" | gzip -9 > "${OUT_CPIO}"
}

# A dynamic image needs the libc loader staged into /lib. WHICH files, from where is a per-libc
# fact, so we source the selected libc's stage-runtime.sh hook rather than branch on the name —
# adding a libc is a new hook, not an edit here.
_stage_libc_runtime() {
  [ "${PKG_LINK:-static}" = dynamic ] || return 0
  : "${LIBC_RECIPE:?rootfs _stage_libc_runtime: LIBC_RECIPE unset (from forge.conf)}"
  local hook; hook="$(dirname "${LIBC_RECIPE}")/stage-runtime.sh"
  [ -f "${hook}" ] || die "libc '${LIBC:-}' has no stage-runtime.sh (needed for a dynamic rootfs): ${hook}"
  mkdir -p "${STAGE}/lib"
  # shellcheck source=/dev/null
  source "${hook}"
}

do_build() {
  : "${STAGE:?rootfs do_build: STAGE unset}"; : "${OUTPUT_DIR:?}"
  : "${BUILD_DIR:?rootfs do_build: BUILD_DIR unset}"; : "${PACKAGES:?rootfs do_build: PACKAGES unset}"
  # INITRAMFS_IMAGE (forge.conf) is keyed by ROOTFS_TAG+link, so static and dynamic land at
  # distinct names — no per-linkage rename needed here.
  local OUT_CPIO="${OUTPUT_DIR}/${INITRAMFS_IMAGE}"
  log "packing rootfs: LIBC=${LIBC:-} PACKAGES='${PACKAGES:-}' (LINK=${PKG_LINK:-static}, BOARD=${ROOTFS_TARGET:-})"
  mkdir -p "${OUTPUT_DIR}"

  # Assemble STAGE from each selected package's per-package dir. Wipe first + merge only the
  # SELECTED dirs so a de-selected package's files never linger; later-listed packages win.
  local PKGSTAGE="${BUILD_DIR}/rootfs/pkgstage" p
  rm -rf "${STAGE}"; mkdir -p "${STAGE}"
  for p in ${PACKAGES}; do
    [ -d "${PKGSTAGE}/${p}" ] || die "rootfs: package '${p}' staged nothing at ${PKGSTAGE}/${p} (build order / pkg-${p} failed?)"
    cp -a "${PKGSTAGE}/${p}/." "${STAGE}/"
  done

  _stage_libc_runtime

  # Subshell: _rootfs_pack does `cd "${STAGE}"` — don't let it leak.
  log "overlay-merge + pack $(basename "${OUT_CPIO}")"
  ( OUT_CPIO="${OUT_CPIO}" \
    DEVTABLE_DEFAULT="${RECIPE_DIR}/rootfs.devs" DEVTABLE="${PRODUCT_DIR}/rootfs.devs" \
    _rootfs_pack )

  printf '\n\033[1;32m[rootfs] DONE\033[0m  %s (%s)  [LIBC=%s PACKAGES=%s LINK=%s]\n' \
    "${OUT_CPIO}" "$(du -h "${OUT_CPIO}" | cut -f1)" "${LIBC:-}" "${PACKAGES:-}" "${PKG_LINK:-static}"
}

do_install() { :; }
