#!/usr/bin/env bash
# classes/make-c.sh — the "make-c" CLASS.
#
# A self-contained provider built with `make -C <src>`: the from-scratch kernel and
# bootloader, whose OWN Makefiles do everything. Only DEFINES the default do_build/
# do_install a recipe gets via `inherit make-c`; the generic runner calls them by name.
#
# The cross toolchain is already on PATH (engine.sh adds it by presence; the engine
# provisioned PKG_HOST_DEPENDS before this recipe ran). Inputs the tasks read (from the
# runner + engine.sh + board.conf): PKG_SRC_DIR (the `make -C` target), LAYER
# (kernel|bootloader — selects the kernel's BOARD= pass), KERNEL_TARGET, PKG_MAKE_GOALS ("all fel"),
# OUTPUT_DIR/KERNEL_DTB/KERNEL_DTB_OVERLAYS/UBOOT_BOARD_DT/BOARD_DIR (kernel DTB step).

# Board files (DTB overlays + board.conf targets) are a build input outside the recipe dir + source, so
# declare them for the recipehash (Yocto file-checksums) — the engine names no board.
PKG_FILEDEPS="${BOARD_DIR}"

# do_build — one `make -C PKG_SRC_DIR` per goal (empty goal list => default goal). Passes
# BOARD=<target> only for the kernel, whose Makefile keys off it.
do_build() {
  : "${PKG_SRC_DIR:?make-c do_build: PKG_SRC_DIR unset}"
  [ -d "${PKG_SRC_DIR}" ] || { echo "make-c: source dir not found: ${PKG_SRC_DIR}" >&2; return 1; }
  command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1 \
    || { echo "make-c: cross compiler '${CROSS_COMPILE}gcc' not on PATH (it's provisioned as a build dependency via virtual/cross-cc)" >&2; return 1; }

  local role="${LAYER:-}"
  local make_vars="" ; [ "${role}" = kernel ] && make_vars="BOARD=${KERNEL_TARGET}"
  local goals="$(recipe_get "${PROVIDER_RECIPE}" PKG_MAKE_GOALS)"

  # Word-split intentional: make_vars is one token (BOARD=t113), goals may be several ("all fel").
  # shellcheck disable=SC2086
  if [ -n "${goals// /}" ]; then
    local g
    for g in ${goals}; do
      log "make -C ${PKG_SRC_DIR##*/} ${make_vars} ${g}"
      make --no-print-directory -C "${PKG_SRC_DIR}" ${make_vars} "${g}"
    done
  else
    log "make -C ${PKG_SRC_DIR##*/} ${make_vars}"
    make --no-print-directory -C "${PKG_SRC_DIR}" ${make_vars}
  fi
}

# do_deploy — the shared PKG_DEPLOY copy (base's deploy_pkg_files) + KERNEL-only: build + deploy the
# board DTB into OUTPUT_DIR, so a full-custom `make image` produces its own DTB with no mainline
# checkout — byte-identical to the mainline-built DTB (same vendored sources + board overlay).
do_deploy() {
  deploy_pkg_files
  [ "${LAYER:-}" = kernel ] || return 0
  : "${PKG_SRC_DIR:?}"; : "${OUTPUT_DIR:?make-c do_deploy: OUTPUT_DIR unset}"
  : "${KERNEL_DTB:?make-c do_deploy: KERNEL_DTB unset (board.conf)}"
  mkdir -p "${OUTPUT_DIR}"
  local make_vars="BOARD=${KERNEL_TARGET}" dtb_overlays="" ov
  for ov in ${KERNEL_DTB_OVERLAYS:-}; do dtb_overlays="${dtb_overlays} ${BOARD_DIR}/${ov}"; done
  log "make -C ${PKG_SRC_DIR##*/} dtb -> ${OUTPUT_DIR}/${KERNEL_DTB}"
  # shellcheck disable=SC2086
  make --no-print-directory -C "${PKG_SRC_DIR}" dtb ${make_vars} \
    UBOOT_BOARD_DT="${UBOOT_BOARD_DT}" \
    DTB_OVERLAYS="$(echo "${dtb_overlays}" | xargs)" \
    DTB_OUT="${OUTPUT_DIR}/${KERNEL_DTB}"
}
