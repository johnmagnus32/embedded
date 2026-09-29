#!/usr/bin/env python3
"""realcode.py — our cpp vs GNU cpp on REAL code: every c-torture program (execute + compile, the flags the torture
runner preprocesses with), every musl 1.2.5 source (its build's preprocessing flags: -std=c99 -ffreestanding
-D_XOPEN_SOURCE=700 -O2 + its include path; .S as assembler-with-cpp) and our libc. Each file is preprocessed by both
(`gcc -E -P` = the oracle) and must come out token-identical (tokdiff rules: spacing between tokens is free). A file
GNU can't preprocess is excluded.

  python3 realcode.py [-j N] [--corpus torture|musl|libc] [--show]      results: $WORK/result-cpp-real.json"""
import glob, json, os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "../../.."))
WORK = os.environ.get("WORK", "/tmp/ctorture")
RESULT = os.path.join(WORK, "result-cpp-real.json")
X = os.environ.get("GNU", os.path.join(ROOT, "projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-"))
OURS = os.path.join(ROOT, "toolchain/cpp/build/cpp")
MUSL = os.path.join(ROOT, "projects/gameboy-v3/image/build/qemu/scratch/musl/src")
sys.path.insert(0, HERE)
from tokdiff import toks_of

def corpora():
    inc = ["-nostdinc", "-isystem", os.path.join(ROOT, "libc/include"), "-isystem", os.path.join(ROOT, "kernel/include/uapi")]
    tort = os.path.join(WORK, "gcc-13.3.0/gcc/testsuite/gcc.c-torture")
    for f in sorted(glob.glob(tort + "/execute/*.c") + glob.glob(tort + "/execute/ieee/*.c") + glob.glob(tort + "/compile/*.c")):
        yield "torture", f, ["-std=gnu11"] + inc, None
    mflags = ["-std=c99", "-nostdinc", "-ffreestanding", "-D_XOPEN_SOURCE=700", "-O2"] + \
             ["-I" + d for d in ("arch/arm", "arch/generic", "obj/src/internal", "src/include", "src/internal", "obj/include", "include")]
    lists = [l.strip() for p in ("/tmp/muslprobe/c.lst", "/tmp/muslprobe/arm.lst") if os.path.exists(p) for l in open(p)]
    for f in lists:
        if f and not f.endswith(".s"): yield "musl", os.path.join(MUSL, f), mflags, MUSL
    linc = ["-nostdinc", "-isystem", os.path.join(ROOT, "libc/include"), "-isystem", os.path.join(ROOT, "kernel/include/uapi"), "-ffreestanding"]
    for f in sorted(glob.glob(os.path.join(ROOT, "libc/src/*.c"))):
        yield "libc", f, linc, None

def run(cmd, cwd):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=60, cwd=cwd)
        return p.returncode, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "timeout"

def one(item):
    corpus, f, flags, cwd = item
    key = corpus + ":" + os.path.relpath(f, cwd or os.path.dirname(f))
    with tempfile.TemporaryDirectory() as w:
        g, o = os.path.join(w, "g.i"), os.path.join(w, "o.i")
        grc, _ = run([X + "gcc", "-E", "-P"] + flags + ["-o", g, f], cwd)
        if grc: return key, {"corpus": corpus, "ours": "excluded", "why": "GNU cpp failed"}
        orc, oerr = run([OURS] + flags + ["-o", o, f], cwd)
        if orc: return key, {"corpus": corpus, "ours": "fail-rejected", "why": oerr.strip().split("\n")[-1][:200]}
        a, b = toks_of(o), toks_of(g)
        if a == b: return key, {"corpus": corpus, "ours": "pass", "why": ""}
        n = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
        return key, {"corpus": corpus, "ours": "fail-output", "why": "at token %d: ours %r, GNU %r" % (n, a[n:n + 5], b[n:n + 5])}

def score(res):
    by = {}
    for v in res.values(): by.setdefault(v["corpus"], []).append(v)
    for c in sorted(by):
        vs = [v for v in by[c] if v["ours"] != "excluded"]; p = sum(v["ours"] == "pass" for v in vs)
        rest = {}
        for v in vs:
            if v["ours"] != "pass": rest[v["ours"]] = rest.get(v["ours"], 0) + 1
        print("  %-8s %5d  pass %5d (%3d%%)   %s   (excluded %d)" % (c, len(vs), p, 100 * p // max(1, len(vs)),
              ", ".join("%s %d" % kv for kv in sorted(rest.items())), len(by[c]) - len(vs)))

def main():
    if "--show" in sys.argv:
        for k, v in sorted(json.load(open(RESULT)).items()):
            if v["ours"] not in ("pass", "excluded"): print("%-50s %-14s %s" % (k, v["ours"], v["why"]))
        return
    if "--score" in sys.argv: score(json.load(open(RESULT))); return
    j = int(sys.argv[sys.argv.index("-j") + 1]) if "-j" in sys.argv else os.cpu_count()
    only = sys.argv[sys.argv.index("--corpus") + 1] if "--corpus" in sys.argv else None
    items = [c for c in corpora() if not only or c[0] == only]
    with ThreadPoolExecutor(j) as ex:
        res = dict(ex.map(one, items))
    json.dump(res, open(RESULT, "w"), indent=0, sort_keys=True)
    score(res)

if __name__ == "__main__":
    main()
