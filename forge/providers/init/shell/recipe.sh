# providers/init/shell/recipe.sh — the minimal PID-1 (INIT=shell): a portable /bin/sh script
# installed verbatim as /init that mounts the API filesystems then `exec /bin/sh`. Kernel-agnostic
# (runs on the from-scratch kernel too), so it's the PID-1 for the from-scratch stack and the kernel
# test harness (kernel/test/golden.sh), where a supervisor isn't needed. The real event-driven
# supervisor is INIT=custom (built from init/).
PKG_NAME=shell-init
PKG_FETCH=none
PKG_ARTIFACT=stage:      # its pkgstage (build/rootfs/pkgstage/init); skips on an unchanged taskhash

do_build() { :; }

do_install() {
  : "${PKG_DEST:?shell-init do_install: PKG_DEST unset}"
  rm -rf "${PKG_DEST}"                       # pkgstage/init is shared across init providers — start clean
  install -D -m 0755 "${RECIPE_DIR}/init" "${PKG_DEST}/init"
}
