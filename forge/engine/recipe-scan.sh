#!/usr/bin/env bash
# recipe-scan.sh — read recipe metadata for the engine. engine.mk calls this via $(shell ...) at parse
# time; it's the ONE place recipe.sh files get parsed on the Make side. A recipe is plain `KEY=value`
# lines. Two queries:
#
#   recipe-scan.sh field     <recipe> <KEY>   -> the value of KEY (last wins), one field
#   recipe-scan.sh providers  <recipe>...     -> a "virtual/x@name" line per virtual each recipe provides
#
# `field` mirrors run-recipe.sh's recipe_get NORMALIZATION (last KEY=, strip a trailing `# comment`,
# strip one surrounding quote layer) but does NOT expand ${VARS} — Make expands those later.
set -euo pipefail

# value of the last `KEY=...` line in a recipe, comment- and quote-stripped.
field() {
  awk -v key="$2" '
    index($0, key "=") == 1 {              # a line starting with KEY=
      v = substr($0, length(key) + 2)      # everything after the "="
      sub(/[ \t]*#.*/, "", v)              # drop a trailing inline comment
      gsub(/^"|"$/, "", v)                 # drop one surrounding quote layer
    }
    END { print v }                        # last match wins (v keeps getting overwritten)
  ' "$1" 2>/dev/null
}

# one "virtual/x@name" line per virtual a recipe provides (name = its dir basename) — the index the
# engine matches PREFERRED_PROVIDER against. PKG_PROVIDES may list several virtuals; each yields a line.
providers() {
  local r v name
  for r in "$@"; do
    name="$(basename "$(dirname "$r")")"
    for v in $(field "$r" PKG_PROVIDES); do echo "${v}@${name}"; done
  done
}

cmd="$1"; shift
"$cmd" "$@"
