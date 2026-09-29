#!/usr/bin/env bash
# inline-emit.sh — WHICH inline definitions become an external definition, compared against GCC. The two dialects are
# opposites, so this is easy to get backwards: under gnu89 (-std=gnu89/c89, or -fgnu89-inline) a plain `inline`
# definition IS emitted and `extern inline` is the inline-only one; from C99 on it is the other way round, and
# __attribute__((gnu_inline)) forces the gnu89 rule per function. Nothing else about the definition may differ.
#   GNU=<cross prefix> CC=<our cc> inline-emit.sh
set -u
G=${GNU:?}; C=${CC:?}
[ -x "${G}gcc" ] || { echo "SKIP inline-emit: no ${G}gcc"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cases=(
  'inline int f(void){return 1;} int g(void){return f();}'
  'static inline int f(void){return 1;} int g(void){return f();}'
  'extern int f(void); inline int f(void){return 1;} int g(void){return f();}'
  'inline int f(void){return 1;} extern int f(void);'
  'inline int f(void); int f(void){return 1;}'
  'inline int f(void){return 1;} inline int f(void);'
  'extern inline int f(void){return 1;} int g(void){return f();}'
  'extern __inline __attribute__((__gnu_inline__)) int f(void){return 1;} int g(void){return f();}'
  '__inline __attribute__((__gnu_inline__)) int f(void){return 1;} int g(void){return f();}'
  'inline __attribute__((used)) int f(void){return 1;}'
  'static int f(void){return 1;} int g(void){return f();}'
)
pass=0; fail=0
for std in "" "-std=gnu89" "-std=c99" "-std=gnu11"; do
  case "$std" in -std=gnu89|-std=c89) ours="-fgnu89-inline" ;; *) ours="" ;; esac
  for c in "${cases[@]}"; do
    printf '%s\n' "$c" > "$W/t.c"
    g=$("${G}gcc" $std -O0 -S -o - "$W/t.c" 2>/dev/null | grep -c '^f:')
    "$C" $ours -o "$W/t.s" "$W/t.c" 2>/dev/null || { echo "FAIL inline-emit [$std] our cc rejected: $c"; fail=$((fail+1)); continue; }
    o=$(grep -c '^f:' "$W/t.s")
    if [ "$g" = "$o" ]; then pass=$((pass+1)); else
      echo "FAIL inline-emit [${std:-default}]: GCC emits f:=$g, ours=$o for: $c"; fail=$((fail+1)); fi
  done
done
echo "inline-emit: $pass identical to GCC, $fail differ"
[ $fail -eq 0 ]
