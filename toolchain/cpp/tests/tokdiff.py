#!/usr/bin/env python3
"""tokdiff.py OURS GNU — compare two preprocessor outputs as TOKEN sequences: whitespace between tokens is free
(ours and GNU space differently), but the tokens must be identical — so a merge (`-` `-1` printed as `--1`), a split,
or a different stringized spelling is a difference. Exit 0 when identical."""
import re, sys
TOK = re.compile(r'''\s*(L?"(?:\\.|[^"\\])*"|L?'(?:\\.|[^'\\])*'|\.?[0-9](?:[eEpP][+-]|[0-9a-zA-Z_.])*|[A-Za-z_][A-Za-z_0-9]*|%:%:|\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|##|<:|:>|<%|%>|%:|\S)''')
def toks_of(path):
    s = open(path, errors="replace").read()
    out, i = [], 0
    while True:
        m = TOK.match(s, i)
        if not m: break
        out.append(m.group(1)); i = m.end()
    return out
if __name__ == "__main__":
    a, b = toks_of(sys.argv[1]), toks_of(sys.argv[2])
    if a == b: sys.exit(0)
    n = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
    print("  first difference at token %d: ours %r, GNU %r" % (n, a[n:n + 6], b[n:n + 6]))
    sys.exit(1)
