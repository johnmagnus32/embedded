#!/usr/bin/env bash
# os/test/run.sh — build-logic tests for the os/ engine (engine.sh).
#
# These verify the BUILD SYSTEM, not any real component: provider resolution, fail-loud config
# handling, the dependency graph, and the recipehash cache — all against a hermetic FIXTURE product
# (os/test/fixture: trivial recipes with no source + no compile). No toolchain, no kernel, no QEMU.
#
# Component behaviour ("does the custom kernel boot", "does the libc work") lives with kernel/, libc/,
# init/ — NOT here. This is the engine's own test suite, the way OpenEmbedded tests bitbake, not a BSP.
#
#   bash os/test/run.sh        # exit 0 iff every assertion passed
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="${HERE}/../engine/engine.sh"
FIX="${HERE}/fixture"

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }

pass=0 fail=0
ok()  { grn "  PASS  $1"; pass=$((pass + 1)); }
bad() { red "  FAIL  $1"; red "        $2"; fail=$((fail + 1)); }

# run <MACHINE-or-empty> <engine-args...> — invoke the engine against a product dir (PRD, default the
# fixture); capture stdout (OUT), stderr (ERR), exit code (RC). Empty MACHINE models "no MACHINE set".
OUT="" ERR="" RC=0
run() {
  local m="$1"; shift
  local o e
  o="$(mktemp)"; e="$(mktemp)"
  MACHINE="${m}" PRODUCT_DIR="${PRD:-${FIX}}" bash "${ENGINE}" "$@" >"${o}" 2>"${e}"
  RC=$?
  OUT="$(cat "${o}")"; ERR="$(cat "${e}")"
  rm -f "${o}" "${e}"
}
has() { printf '%s' "$1" | grep -qF -- "$2"; }   # substring present?

echo "== os/ engine build-logic tests =="

# --- config handling: fail loud, never a magical default ---------------------
run "" resolve-dependencies t-init
{ [ "${RC}" -eq 0 ]; } \
  && ok "resolve-dependencies is quiet without MACHINE (parse-time; Make runs it per recipe)" \
  || bad "resolve-dependencies without MACHINE should exit 0" "rc=${RC} err=${ERR}"

run "" execute-recipe t-init
{ [ "${RC}" -ne 0 ] && has "${ERR}" "MACHINE unset"; } \
  && ok "execute-recipe fails loud without MACHINE" \
  || bad "execute-recipe without MACHINE should die 'MACHINE unset'" "rc=${RC} err=${ERR}"

run nosuch execute-recipe t-init
{ [ "${RC}" -ne 0 ] && has "${ERR}" "no machine config at"; } \
  && ok "an unknown MACHINE fails loud" \
  || bad "MACHINE=nosuch should die 'no machine config at'" "rc=${RC} err=${ERR}"

# --- provider resolution + dependency graph ----------------------------------
run testmachine resolve-dependencies t-init
{ [ "$(printf '%s' "${OUT}" | tr -s ' ' | sed 's/ *$//')" = "make" ]; } \
  && ok "leaf recipe resolves to just the 'make' barrier" \
  || bad "t-init deps should be 'make'" "out='${OUT}'"

run testmachine resolve-dependencies t-app
{ has "${OUT}" "t-libc" && has "${OUT}" "t-cc"; } \
  && ok "target+libc resolves the libc provider (t-libc) + implied cross-cc (t-cc)" \
  || bad "t-app deps should include t-libc + t-cc" "out='${OUT}'"

run testmachine execute-recipe bogus
{ [ "${RC}" -ne 0 ] && has "${ERR}" "no recipe for"; } \
  && ok "an unknown recipe name fails loud" \
  || bad "execute-recipe bogus should die 'no recipe for'" "rc=${RC} err=${ERR}"

# a missing compiler provider must fail loud, never a silent empty toolchain. Build a throwaway product
# whose local.conf drops the cross-cc provider (recipes shared via symlink; machine reused).
tmp="$(mktemp -d)"
mkdir -p "${tmp}/conf/machine"
ln -s "${FIX}/recipes-test" "${tmp}/recipes-test"
cp "${FIX}/conf/machine/testmachine.conf" "${tmp}/conf/machine/"
grep -v '^PROVIDER_cross_cc=' "${FIX}/conf/local.conf" > "${tmp}/conf/local.conf"
PRD="${tmp}" run testmachine execute-recipe t-init
rm -rf "${tmp}"
{ [ "${RC}" -ne 0 ] && has "${ERR}" "CROSS_COMPILE empty"; } \
  && ok "a missing cross-cc provider fails loud at build (no empty toolchain)" \
  || bad "dropping the cross-cc provider should die 'CROSS_COMPILE empty'" "rc=${RC} err=${ERR}"

# --- recipehash cache (hermetic build dir) -----------------------------------
rm -rf "${FIX}/build"

run testmachine execute-recipe t-init
{ has "${OUT}${ERR}" "T-BUILD-RAN t-init"; } \
  && ok "cache: first run executes the recipe's tasks" \
  || bad "first execute-recipe should run do_build" "rc=${RC} out='${OUT}' err='${ERR}'"

run testmachine execute-recipe t-init
{ has "${OUT}${ERR}" "cached" && ! has "${OUT}${ERR}" "T-BUILD-RAN"; } \
  && ok "cache: an unchanged recipe is skipped (cached)" \
  || bad "second execute-recipe should be cached, not re-run" "rc=${RC} out='${OUT}' err='${ERR}'"

recipe="${FIX}/recipes-test/t-init/recipe.sh"
cp -f "${recipe}" "${recipe}.bak"
printf '\n# cache-bust %s\n' "$$" >> "${recipe}"
run testmachine execute-recipe t-init
mv -f "${recipe}.bak" "${recipe}"
{ has "${OUT}${ERR}" "T-BUILD-RAN t-init"; } \
  && ok "cache: editing the recipe busts the stamp and re-runs it" \
  || bad "editing the recipe should re-run do_build" "rc=${RC} out='${OUT}' err='${ERR}'"

rm -rf "${FIX}/build"

echo
if [ "${fail}" -eq 0 ]; then
  grn "os/test: ${pass}/${pass} passed."
  exit 0
fi
red "os/test: ${fail} FAILED (${pass} passed)."
exit 1
