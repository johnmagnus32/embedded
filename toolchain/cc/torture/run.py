#!/usr/bin/env python3
"""Run GCC 13.3's gcc.c-torture/execute suite (1627 self-checking programs: exit 0 = pass, abort() = fail)
through OUR toolchain (cc -> as -> ld, linked with our libc/src built by our cc + torture/rt.c) on
qemu-system-arm -M virt with semihosting. Every test is also built by the reference GNU toolchain (gcc -O0,
GNU ld, libgcc) with the same libc + runtime and run the same way; only tests the ORACLE passes are scored, so
harness/environment limits never count against us. Tests are tiered by the language features they use.

usage: run.py [-j N] [--only NAME...] [-v]        results -> $CTORTURE_WORK/result.json
"""
import os, re, sys, json, glob, subprocess, tempfile, collections, concurrent.futures as cf

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "../../.."))
WORK = os.environ.get("CTORTURE_WORK", "/tmp/ctorture")
SUITE = os.path.join(WORK, "gcc-13.3.0/gcc/testsuite/gcc.c-torture/execute")
X = os.environ.get("GNU", os.path.join(ROOT, "projects/gameboy-v3/image/build/qemu/toolchain-gcc/bin/arm-forge-linux-gnueabihf-"))
CC = os.path.join(ROOT, "toolchain/cc/build/cc"); AS = os.path.join(ROOT, "toolchain/as/build/as"); LD = os.path.join(ROOT, "toolchain/ld/build/ld")
LIBC_SRC = ["string.c", "stdlib.c", "stdio.c", "printf.c", "malloc.c", "lldiv.c"]
INC = ["-nostdinc", "-isystem", os.path.join(ROOT, "libc/include"), "-isystem", os.path.join(ROOT, "kernel/include/uapi")]
CPP_FLAGS = ["-E"] + INC + ["-std=gnu89", "-w", "-D__TORTURE__"]
GCC_FLAGS = ["-O0", "-marm", "-mcpu=cortex-a7", "-std=gnu89", "-w", "-fno-builtin-printf"]
QEMU = ["qemu-system-arm", "-M", "virt", "-cpu", "cortex-a7", "-m", "256", "-nographic", "-semihosting", "-net", "none", "-kernel"]

def run(cmd, timeout=60, **kw):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout, **kw)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "timeout"

def last(err): return (err.strip().splitlines() or ["(no message)"])[-1][:200]

def build_runtime():
    """libc + rt.c objects, once per toolchain side."""
    rt = os.path.join(WORK, "rt"); os.makedirs(rt, exist_ok=True)
    srcs = [os.path.join(ROOT, "libc/src", f) for f in LIBC_SRC] + [os.path.join(HERE, "rt.c")]
    ours, gnu = [], []
    for s in srcs:
        b = os.path.join(rt, os.path.splitext(os.path.basename(s))[0])
        rc, _, e = run([X + "gcc", "-E"] + INC + ["-std=gnu11", "-o", b + ".i", s])   # libc is modern C; tests are gnu89
        if rc: sys.exit("runtime: preprocess %s: %s" % (s, last(e)))
        for cmd in ([CC, "-o", b + ".s", b + ".i"], [AS, "-o", b + ".ours.o", b + ".s"]):
            rc, _, e = run(cmd)
            if rc: sys.exit("runtime: OUR toolchain failed on %s: %s" % (s, last(e)))
        rc, _, e = run([X + "gcc"] + [f for f in GCC_FLAGS if not f.startswith("-std")] + ["-std=gnu11", "-c", "-o", b + ".gnu.o", s] + INC)
        if rc: sys.exit("runtime: GCC failed on %s: %s" % (s, last(e)))
        ours.append(b + ".ours.o"); gnu.append(b + ".gnu.o")
    crt_o = os.path.join(rt, "crt.o"); crt_g = os.path.join(rt, "crt_gnu.o")
    if run([AS, "-o", crt_o, os.path.join(ROOT, "toolchain/cc/tests/crt.s")])[0]: sys.exit("runtime: crt.s")
    if run([X + "as", "-mcpu=cortex-a7", "-mfpu=vfpv4", "-o", crt_g, os.path.join(HERE, "crt_gnu.s")])[0]: sys.exit("runtime: crt_gnu.s")
    libgcc = run([X + "gcc", "-print-libgcc-file-name"])[1].strip()
    return [crt_o] + ours, [crt_g] + gnu, libgcc

def tier(src):
    """Feature area, from the source text (reported, not hidden)."""
    if re.search(r"\b(_Complex|__complex__|_Complex_I)\b|\b__real__\b|\b__imag__\b", src): return "complex"
    if re.search(r"\b(float|double|long\s+double|__fp16|_Float\d+)\b", src): return "floating-point"
    if re.search(r"vector_size|__attribute__\s*\(\(\s*(__)?vector", src): return "vector-ext"
    if re.search(r"__int128", src): return "int128"
    return "core"

def one(path, ours_rt, gnu_rt, libgcc):
    name = os.path.splitext(os.path.basename(path))[0]
    src = open(path, errors="replace").read()
    t = tier(src)
    with tempfile.TemporaryDirectory() as td:
        i = os.path.join(td, "t.i")
        rc, _, e = run([X + "gcc"] + CPP_FLAGS + ["-o", i, path])
        if rc: return name, {"tier": t, "ours": "n/a", "gnu": "preprocess-fail", "why": last(e)}
        # oracle: pure GNU
        g_o = os.path.join(td, "g.o"); g_elf = os.path.join(td, "g.elf")
        rc, _, e = run([X + "gcc"] + GCC_FLAGS + ["-c", "-x", "cpp-output", "-o", g_o, i])
        if rc: gnu = "compile-fail"
        else:
            rc, _, e = run([X + "ld", "-z", "noexecstack", "-Ttext=0x40000000", "-o", g_elf] + [gnu_rt[0], g_o] + gnu_rt[1:] + [libgcc])
            if rc: gnu = "link-fail"
            else: rc, _, _ = run(QEMU + [g_elf], timeout=20); gnu = "pass" if rc == 0 else ("timeout" if rc == 124 else "fail(%d)" % rc)
        # ours
        s = os.path.join(td, "t.s"); o = os.path.join(td, "t.o"); elf = os.path.join(td, "t.elf")
        rc, _, e = run([CC, "-o", s, i])
        if rc: return name, {"tier": t, "ours": "cc-fail", "gnu": gnu, "why": last(e)}
        rc, _, e = run([AS, "-o", o, s])
        if rc: return name, {"tier": t, "ours": "as-fail", "gnu": gnu, "why": last(e)}
        rc, _, e = run([LD, "-Ttext", "0x40000000", "-o", elf] + [ours_rt[0], o] + ours_rt[1:])
        if rc: return name, {"tier": t, "ours": "link-fail", "gnu": gnu, "why": last(e)}
        rc, out, _ = run(QEMU + [elf], timeout=20)
        ours = "pass" if rc == 0 else ("timeout" if rc == 124 else "fail(%d)" % rc)
        return name, {"tier": t, "ours": ours, "gnu": gnu, "why": "" if ours == "pass" else out[-200:]}

def main():
    args = sys.argv[1:]; jobs = 32; only = None; verbose = "-v" in args
    if "-j" in args: jobs = int(args[args.index("-j") + 1])
    if "--only" in args: only = set(args[args.index("--only") + 1:])
    tests = sorted(glob.glob(os.path.join(SUITE, "*.c")))
    if only: tests = [t for t in tests if os.path.splitext(os.path.basename(t))[0] in only]
    ours_rt, gnu_rt, libgcc = build_runtime()
    res = {}
    with cf.ThreadPoolExecutor(jobs) as ex:
        for name, r in ex.map(lambda p: one(p, ours_rt, gnu_rt, libgcc), tests):
            res[name] = r
            if verbose: print("%-28s %-15s gnu=%-12s %s" % (name, r["ours"], r["gnu"], r["why"].replace("\n", " ")[:90]))
    json.dump(res, open(os.path.join(WORK, "result.json"), "w"), indent=0)
    score(res)

def score(res):
    valid = {k: v for k, v in res.items() if v["gnu"] == "pass"}
    print("tests=%d  oracle(GNU) passes=%d  (excluded: %s)" % (len(res), len(valid),
          ", ".join("%s %d" % kv for kv in sorted(collections.Counter(v["gnu"] for v in res.values() if v["gnu"] != "pass").items()))))
    by = collections.defaultdict(collections.Counter)
    for v in valid.values(): by[v["tier"]][v["ours"]] += 1
    for t in sorted(by):
        c = by[t]; tot = sum(c.values())
        print("  %-15s %4d  pass %4d (%3d%%)   %s" % (t, tot, c["pass"], 100 * c["pass"] // tot, ", ".join("%s %d" % kv for kv in sorted(c.items()) if kv[0] != "pass")))

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--score": score(json.load(open(os.path.join(WORK, "result.json"))))
    else: main()
