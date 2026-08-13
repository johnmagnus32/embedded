# providers/libc/custom/cc-profile.sh — gv3libc's CC/link contract (sourced by a compile class).
#
# gv3libc's real profile self-locates its headers/crt/linker-script relative to the libc source
# tree (repo-root libc/), so it lives THERE (libc/libc-profile.sh) beside the code it references.
# This is the thin provider-recipe shim that points the engine at it — the compile class sources
# "<selected libc recipe dir>/cc-profile.sh" (via LIBC_CC_PROFILE) UNCONDITIONALLY, with no
# per-libc branch in the engine. REPO_ROOT is in the env (forge.conf / run-recipe.sh).
# shellcheck source=/dev/null
source "${REPO_ROOT}/libc/libc-profile.sh"
