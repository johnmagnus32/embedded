#!/usr/bin/env bash
# classes/kconfig.sh — the "kconfig" CLASS: the shared kconfig CONFIGURE MECHANISM (Buildroot's
# pkg-kconfig.mk / Yocto's cml1.bbclass analogue). A recipe `inherit kconfig` to get these
# functions in scope, then drives them from its own do_build:
#     make <defconfig>  ->  run the recipe's FIXUP callback  ->  normalize  [-> verify fragments]
# The MECHANISM is generic; the DATA + build/install POLICY are per-recipe (each kconfig
# component — Linux, U-Boot, BusyBox — supplies its own do_build/do_install, same as Buildroot's
# per-package *_BUILD_CMDS). This class deliberately defines NO do_build/do_install: it is pure
# mechanism, so every inheritor overrides the tasks with its own inline do_build.
#
# Sourced into the node shell by `inherit kconfig` (run-recipe.sh's inherit()), which runs when
# the recipe is sourced — BEFORE do_build. The functions are then in scope for the recipe's
# inline do_build (busybox, and kernel/u-boot alike). export -f is belt-and-suspenders for any
# child bash that do_build might spawn.

# kconfig_normalize [olddefconfig|oldconfig] — re-resolve .config after edits. olddefconfig
# (default every NEW symbol) is the norm; BusyBox ships only interactive `oldconfig`, fed
# EOF. stdin is /dev/null so a stray prompt can't hang the build.
kconfig_normalize() {
  case "${1:-olddefconfig}" in
    oldconfig) make oldconfig </dev/null >/dev/null ;;
    *)         make "${1:-olddefconfig}" </dev/null >/dev/null ;;
  esac
}

# kconfig_configure <defconfig> <fixup_fn> [normalize] — seed <defconfig>, run the FIXUP
# callback, normalize. A provider needing an ordering-sensitive second batch (U-Boot's
# MTD-before-SPI_FLASH) edits after this and calls kconfig_normalize again.
kconfig_configure() {
  local defconfig="$1" fixup_fn="$2" normalize="${3:-olddefconfig}"
  make "${defconfig}" >/dev/null
  "${fixup_fn}"
  kconfig_normalize "${normalize}"
}

# apply_config_fragment <fragment-file> — apply a .config FRAGMENT via the tree's own
# scripts/config, one symbol per line, IN FILE ORDER. Line order is load-bearing: a menu
# gated behind another symbol (U-Boot's SPI-flash menu is `if MTD`) needs the gate enabled
# FIRST, so list CONFIG_MTD=y before CONFIG_SPI_FLASH=y. (scripts/config edits in place;
# merge_config.sh was avoided because it appends at EOF and reorders.) Recognizes the three
# kconfig line shapes; blank/bare-comment lines are skipped.
apply_config_fragment() {
  local frag="$1" line sym val
  [ -f "${frag}" ] || { echo "apply_config_fragment: fragment not found: ${frag}" >&2; return 1; }
  while IFS= read -r line; do
    case "${line}" in
      '# CONFIG_'*' is not set')
        sym="${line#\# CONFIG_}"; sym="${sym% is not set}"; ./scripts/config --disable "${sym}" ;;
      'CONFIG_'*'=y')
        sym="${line#CONFIG_}"; sym="${sym%=y}"; ./scripts/config --enable "${sym}" ;;
      'CONFIG_'*'='*)
        sym="${line#CONFIG_}"; val="${sym#*=}"; sym="${sym%%=*}"
        case "${val}" in
          '"'*'"') val="${val#\"}"; val="${val%\"}"; ./scripts/config --set-str "${sym}" "${val}" ;;
          *)       ./scripts/config --set-val "${sym}" "${val}" ;;
        esac ;;
      ''|'#'*) : ;;   # blank / bare comment: skip
      *) echo "apply_config_fragment: unrecognized line in ${frag}: ${line}" >&2; return 1 ;;
    esac
  done < "${frag}"
}

# verify_config_fragment <fragment-file> — after apply + normalize, assert every forced
# symbol actually took (olddefconfig silently drops a symbol whose deps aren't met).
# Enables/values must be present verbatim; a disable must merely NOT be enabled (an
# unreachable symbol may vanish, which counts as satisfied). Reads .config in the cwd.
verify_config_fragment() {
  local frag="$1" line sym val
  [ -f .config ] || { echo "verify_config_fragment: no .config in $(pwd)" >&2; return 1; }
  while IFS= read -r line; do
    case "${line}" in
      '# CONFIG_'*' is not set')
        sym="${line#\# CONFIG_}"; sym="${sym% is not set}"
        if grep -q "^CONFIG_${sym}=y$" .config; then
          echo "verify_config_fragment: CONFIG_${sym} is still ENABLED (fragment ${frag} wanted it off)" >&2; return 1
        fi ;;
      'CONFIG_'*'=y')
        sym="${line#CONFIG_}"; sym="${sym%=y}"
        if ! grep -q "^CONFIG_${sym}=y$" .config; then
          echo "verify_config_fragment: CONFIG_${sym}=y did not stick (deps unmet? — fragment ${frag})" >&2; return 1
        fi ;;
      'CONFIG_'*'='*)
        sym="${line#CONFIG_}"; val="${sym#*=}"; sym="${sym%%=*}"
        if ! grep -q "^CONFIG_${sym}=${val}$" .config; then
          echo "verify_config_fragment: CONFIG_${sym}=${val} did not stick (fragment ${frag})" >&2; return 1
        fi ;;
      ''|'#'*) : ;;
    esac
  done < "${frag}"
  return 0   # reached only if every checked symbol held (the loop's last status is not a verdict)
}
export -f kconfig_normalize kconfig_configure apply_config_fragment verify_config_fragment
