#!/usr/bin/env bash
# recipe-scan.sh — read recipe metadata for the engine. engine.mk calls this via $(shell ...) at parse
# time; it's the ONE place recipe.sh files get parsed on the Make side. A recipe is plain `KEY=value`
# lines. Three queries:
#
#   recipe-scan.sh field <recipe> <KEY>     -> the value of KEY (last wins), one field
#   recipe-scan.sh providers <recipe>...    -> a "virtual@alias@recipe" line per virtual each provides
#   recipe-scan.sh classes   <recipe>...    -> a "name|PKG_CLASS" line per recipe (name = dir basename)
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

# one "virtual@alias@recipe" line per (virtual, alias) a recipe declares. PKG_PROVIDES may list several
# virtuals (space-separated); PKG_ALIAS is the single selector value they're chosen by.
providers() {
  local r v alias provides
  for r in "$@"; do
    provides=$(field "$r" PKG_PROVIDES)
    alias=$(field "$r" PKG_ALIAS)
    [ -n "$provides" ] && [ -n "$alias" ] || continue
    for v in $provides; do echo "$v@$alias@$r"; done
  done
}

# one "name|PKG_CLASS" line per recipe — the name is the recipe's directory basename.
classes() {
  local r
  for r in "$@"; do
    echo "$(basename "$(dirname "$r")")|$(field "$r" PKG_CLASS)"
  done
}

cmd="$1"; shift
"$cmd" "$@"
