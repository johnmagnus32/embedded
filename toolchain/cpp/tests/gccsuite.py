#!/usr/bin/env python3
"""gccsuite.py — GCC 13.3's gcc.dg/cpp preprocessor tests as a DIFFERENTIAL gate for our cpp.

Each test is preprocessed by GNU cpp (`gcc -E -P`, the oracle) and by ours, with the test's own dg-options and the
same include paths. GNU rejects it (a #error, an invalid paste, a bad directive...) -> ours must reject it too; GNU
accepts it -> our output must be token-identical (tokens.py rules: spacing between tokens is free, a merged or split
token is a difference). A test whose options we don't implement (our cpp: "unsupported option") is n/a, never a pass.

  GNU=<cross prefix> python3 gccsuite.py [-j N] [--score] [--show TIER|NAME]
Results: $WORK/result-cpp.json. Tiers: the test's dg-do (preprocess / compile / run) and asm (.S)."""
import json, os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "../../.."))
WORK = os.environ.get("WORK", "/tmp/ctorture")
SUITE = os.path.join(WORK, "gcc-13.3.0/gcc/testsuite/gcc.dg/cpp")
RESULT = os.path.join(WORK, "result-cpp.json")
X = os.environ.get("GNU", os.path.join(ROOT, "projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-"))
OURS = os.path.join(ROOT, "toolchain/cpp/build/cpp")
MUSL_INC = os.path.join(ROOT, "projects/gameboy-v3/image/build/qemu/libc/stage-musl-static/usr/include")
sys.path.insert(0, HERE)
from tokdiff import toks_of

def run(cmd, timeout=20, cwd=None, env=None):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout, cwd=cwd, env=env)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "timeout"

GCC_INC = run([X + "gcc", "-print-file-name=include"])[1].strip()

def dg_options(src):
    """The test's dg-options (+ dg-additional-options) that apply to this target: no selector, or one naming
    arm / linux / *-*-*. A selector we can't evaluate (effective-target) makes the directive not apply."""
    opts = []
    for m in re.finditer(r'\{\s*dg-(?:additional-)?options\s+"([^"]*)"\s*(\{[^}]*\})?\s*\}', src):
        sel = m.group(2)
        if sel and not re.search(r'target\s+(\*-\*-\*|arm\*?-\*-\*|\*-\*-linux\*)', sel): continue
        opts += m.group(1).split()
    return opts

def tier_of(path, src):
    if path.endswith(".S"): return "asm"
    m = re.search(r'dg-do\s+(\w+)', src)
    return m.group(1) if m else "preprocess"

def last(s):
    ls = [l for l in s.strip().split("\n") if l.strip()]
    return ls[-1][:200] if ls else ""

def one(path):
    name = os.path.basename(path)
    src = open(path, errors="replace").read()
    t = tier_of(path, src)
    opts = dg_options(src)
    inc = ["-nostdinc", "-isystem", MUSL_INC, "-isystem", GCC_INC]
    lang = ["-x", "assembler-with-cpp"] if path.endswith(".S") else []
    d = os.path.dirname(path)
    env = dict(os.environ, SOURCE_DATE_EPOCH="630333296")   # __DATE__ / __TIME__ the same for both (GCC honors it)
    for m in re.finditer(r'dg-set-compiler-env-var\s+(\w+)\s+"([^"]*)"', src): env[m.group(1)] = m.group(2)
    with tempfile.TemporaryDirectory() as w:
        g_out, o_out = os.path.join(w, "g.i"), os.path.join(w, "o.i")
        grc, _, gerr = run([X + "gcc", "-E", "-P"] + lang + opts + inc + ["-o", g_out, path], cwd=d, env=env)
        if grc == 124: return name, {"tier": t, "ours": "n/a", "gnu": "timeout", "why": ""}
        orc, _, oerr = run([OURS] + lang + opts + inc + ["-o", o_out, path], cwd=d, env=env)
        if orc and "unsupported option" in oerr:
            return name, {"tier": t, "ours": "n/a-option", "gnu": "ok" if grc == 0 else "reject", "why": last(oerr)}
        if grc:
            ours = "pass" if orc else "fail-accepted"
            return name, {"tier": t, "ours": ours, "gnu": "reject", "why": "" if orc else last(gerr)}
        if orc:
            return name, {"tier": t, "ours": "fail-rejected" if orc != 124 else "timeout", "gnu": "ok", "why": last(oerr)}
        a, b = toks_of(o_out), toks_of(g_out)
        if a == b: return name, {"tier": t, "ours": "pass", "gnu": "ok", "why": ""}
        n = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
        return name, {"tier": t, "ours": "fail-output", "gnu": "ok", "why": "at token %d: ours %r, GNU %r" % (n, a[n:n + 5], b[n:n + 5])}

def score(res):
    tiers = {}
    for v in res.values():
        tiers.setdefault(v["tier"], []).append(v)
    print("tests=%d  (GNU: %d accept, %d reject, %d timeout)" % (len(res), sum(v["gnu"] == "ok" for v in res.values()),
          sum(v["gnu"] == "reject" for v in res.values()), sum(v["gnu"] == "timeout" for v in res.values())))
    for t in sorted(tiers):
        vs = [v for v in tiers[t] if v["gnu"] != "timeout"]
        p = sum(v["ours"] == "pass" for v in vs)
        rest = {}
        for v in vs:
            if v["ours"] != "pass": rest[v["ours"]] = rest.get(v["ours"], 0) + 1
        print("  %-12s %4d  pass %4d (%3d%%)   %s" % (t, len(vs), p, 100 * p // max(1, len(vs)), ", ".join("%s %d" % kv for kv in sorted(rest.items()))))

def main():
    if "--score" in sys.argv:
        score(json.load(open(RESULT))); return
    if "--show" in sys.argv:
        key = sys.argv[sys.argv.index("--show") + 1]; res = json.load(open(RESULT))
        for k, v in sorted(res.items()):
            if v["ours"] != "pass" and v["gnu"] != "timeout" and (key in (k, v["tier"], v["ours"], "all")):
                print("%-28s %-10s %-14s gnu=%-6s %s" % (k, v["tier"], v["ours"], v["gnu"], v["why"]))
        return
    j = int(sys.argv[sys.argv.index("-j") + 1]) if "-j" in sys.argv else os.cpu_count()
    tests = sorted(os.path.join(SUITE, f) for f in os.listdir(SUITE) if f.endswith((".c", ".S")))
    with ThreadPoolExecutor(j) as ex:
        res = dict(ex.map(one, tests))
    json.dump(res, open(RESULT, "w"), indent=0, sort_keys=True)
    score(res)

if __name__ == "__main__":
    main()
