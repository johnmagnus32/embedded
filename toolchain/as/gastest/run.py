#!/usr/bin/env python3
"""Run binutils 2.42 gas/testsuite/gas/arm dump tests (.d) against OUR as, mirroring binutils' run_dump_test +
regexp_diff. The same harness runs GNU as 2.42 (same version as the suite) as a self-check: a test GNU as
fails here is a harness limitation, reported separately — never counted against us.

usage: GNU=<binutils-2.42 prefix> run.py [--as ours|gnu] [--only NAME...] [-v]   (see ../Makefile: gas-suite)
"""
import os, re, subprocess, sys, glob, fnmatch, collections, tempfile, warnings
warnings.simplefilter("ignore", FutureWarning)

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("GASTEST_WORK", "/tmp/gastest")
SUITE = os.path.join(WORK, "binutils-2.42/gas/testsuite/gas/arm")
X = os.environ.get("GNU", "")   # GNU binutils 2.42 prefix (objdump/readelf/nm, and as for --as gnu)
OURS = os.path.join(HERE, "..", "build", "as")
TRIPLET = "arm-unknown-linux-gnueabihf"   # what `#target/#notarget/#skip` patterns are matched against

def parse_d(path):
    opts = collections.defaultdict(list); body = []; in_hdr = True
    for ln in open(path, errors="replace").read().splitlines():
        m = re.match(r"^#\s*([a-z_]+)\s*:\s*(.*)$", ln) if in_hdr else None
        if m: opts[m.group(1)].append(m.group(2).strip()); continue
        if in_hdr and (ln.startswith("#") and not ln.startswith("#...") and not ln.startswith("#pass")): continue   # header comments
        in_hdr = False; body.append(ln)
    return opts, body

TCL_PROCS = {"is_elf_format": True, "is_pe_format": False, "is_aout_format": False, "uses_genelf": True,
             "is_generic_elf": False, "is_linux": True}
def target_matches(spec):
    """A `#target/#notarget/#skip` value: whitespace-separated globs and/or [tcl_proc] calls — any match."""
    for tok in re.findall(r"\[[^\]]*\]|\S+", spec):
        if tok.startswith("["):
            name = tok.strip("[] ").split()[0].lstrip("!")
            val = TCL_PROCS.get(name)
            if val is None: continue
            if tok.strip("[] ").startswith("!"): val = not val
            if val: return True
        elif fnmatch.fnmatch(TRIPLET, tok): return True
    return False

def regexp_diff(out_lines, exp_lines):
    """binutils regexp_diff: blank output lines ignored; blank/#comment expected lines ignored; `#...` skips
    output lines until the next expected line matches; `#pass` ends with success; each line is ^re$."""
    a = [l for l in out_lines if l.strip() != ""]; ai = 0; bi = 0; b = exp_lines
    while True:
        skip = False
        while bi < len(b) and (b[bi].strip() == "" or b[bi].startswith("#")):
            if b[bi].startswith("#pass"): return True, ""
            if b[bi].startswith("#..."): skip = True
            bi += 1
        if bi >= len(b):
            return (True, "") if (skip or ai >= len(a)) else (False, "extra output: %r" % a[ai])
        pat = b[bi]
        try: rx = re.compile("^" + pat + "$")
        except re.error as e: return None, "harness: bad regex %r (%s)" % (pat, e)
        if skip:
            while ai < len(a) and not rx.match(a[ai]): ai += 1
        if ai >= len(a): return False, "missing: %r" % pat
        if not rx.match(a[ai]): return False, "want %r\n      got  %r" % (pat, a[ai])
        ai += 1; bi += 1

def tier(opts, src_text):
    """Which feature area a test exercises (for honest scoping; every test is still run + reported)."""
    flags = " ".join(opts.get("as", []))
    s = src_text.lower()
    if opts.get("error") or opts.get("error_output") or opts.get("warning") or opts.get("warning_output"): return "diagnostics"
    # flags that CHANGE the output and that our as (no options; fixed target) doesn't take
    if re.search(r"--fix-v4bx|-mfix-v4bx|-mccs\b|-mfp16-format", flags): return "needs-flag"
    if "-eb" in flags.lower() or "-mbig-endian" in flags: return "big-endian"
    if re.search(r"-march=armv(6s?-m|7e?-m|8(\.1)?-m)|-mcpu=cortex-m|armv8\.1-m|\bmve\b", flags + s): return "M-profile"
    if re.search(r"-mthumb\b|^\s*\.(thumb|code\s+16)\b|\.thumb_func", flags + "\n" + s, re.M): return "thumb"
    if re.search(r"-mfpu|\+(fp|simd|mve|crypto|dotprod|fp16|bf16|i8mm)|\.fpu\b|\bv(add|mul|ld[1-4]|st[1-4]|mov|cvt|cmp)\b|\bneon\b|iwmmxt|maverick|\bfpa\b|-mfloat-abi", flags + "\n" + s): return "fp/simd/coproc-ext"
    if re.search(r"-march=armv8|-march=armv9|-mcpu=cortex-a(3[2-9]|5\d|7[2-9])|\.arch\s+armv8", flags + "\n" + s): return "armv8+"
    return "core-arm"

def v7flags(flags):
    """Our as is fixed ARMv7-A (ARM state). Force GNU as to the same target: drop its arch/cpu selection and use
    -march=armv7-a. A test GNU as FAILS this way depends on a different architecture than ours."""
    keep = [f for f in flags if not f.startswith(("-march", "-mcpu", "-mfpu", "-mthumb", "-mfloat-abi", "-mimplicit-it"))]
    return keep + ["-march=armv7-a"]

def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return p.returncode, p.stdout, p.stderr

def one(dpath, which):
    opts, body = parse_d(dpath)
    base = os.path.splitext(os.path.basename(dpath))[0]
    for key in ("notarget", "skip"):
        for v in opts.get(key, []):
            if target_matches(v): return "skipped-target", key + ": " + v, None
    for v in opts.get("target", []):
        if not target_matches(v): return "skipped-target", "target: " + v, None
    src = os.path.join(SUITE, opts["source"][0].split()[0] if opts.get("source") else base + ".s")
    if not os.path.exists(src): return "harness", "no source " + src, None
    t = tier(opts, open(src, errors="replace").read())
    if any(k in opts for k in ("ld", "objcopy_objects", "objcopy_linked_file", "objcopy")): return "harness-unsupported", "multi-step (ld/objcopy)", t
    flags = " ".join(opts.get("as", [])).split()
    wants_error = bool(opts.get("error") or opts.get("error_output"))
    if wants_error:   # diagnostics test: the input must be REJECTED (message text is GNU-specific; we check rejection)
        arch_gated = any(f.startswith(("-march", "-mcpu", "-mfpu", "-mthumb", "-mfloat", "-mimplicit", "-mwarn", "-mno-", "-meabi", "-EB")) for f in flags)
        if not os.path.exists(src): return "harness", "no source", t
        with tempfile.TemporaryDirectory() as td:
            obj = os.path.join(td, "t.o")
            rc, _, err = run(([X + "as"] + flags if which == "gnu" else [OURS]) + ["-o", obj, src])
        tt = "diag-arch-gated" if arch_gated else "diag-plain"
        if re.search(r"--fix-v4bx|-mfix-v4bx|-mccs\b|-mfp16-format", " ".join(flags)): tt = "needs-flag"   # depends on a flag we don't take
        if which != "gnu" and base.startswith(("cmdline-", "bfloat16-cmdline")): return "n/a-cmdline", "tests as' command-line flags", tt
        return ("pass", "", tt) if rc != 0 else ("fail-accepted", "accepted invalid input", tt)
    dump = next(((k, opts[k][0]) for k in ("objdump", "readelf", "nm") if opts.get(k)), None)
    if not dump: return "harness-unsupported", "no dump step (%s)" % ",".join(k for k in opts if k not in ("name", "as", "source")), t
    with tempfile.TemporaryDirectory() as td:
        obj = os.path.join(td, "t.o")
        if which == "gnu": rc, _, err = run([X + "as"] + flags + ["-o", obj, src])
        elif which == "gnu7": rc, _, err = run([X + "as"] + v7flags(flags) + ["-o", obj, src])
        else: rc, _, err = run([OURS, "-o", obj, src])
        if rc != 0: return "fail-asm", (err.strip().splitlines() or ["(no message)"])[-1][:200], t
        rc, out, err = run([X + dump[0]] + dump[1].split() + [obj])
        if rc != 0 and not out: return "harness", dump[0] + ": " + err.strip()[:200], t
    ok, why = regexp_diff(out.splitlines(), body)
    if ok is None: return "harness", why, t
    return ("pass", "", t) if ok else ("fail-output", why, t)

def main():
    if not X: sys.exit("set GNU=<prefix of GNU binutils 2.42> (objdump/readelf/nm are the reference dumpers)")
    which = "ours"; only = []; verbose = "-v" in sys.argv
    args = [a for a in sys.argv[1:] if a != "-v"]
    if "--as" in args: which = args[args.index("--as") + 1]
    if "--only" in args: only = args[args.index("--only") + 1:]
    ds = sorted(glob.glob(os.path.join(SUITE, "*.d")))
    if only: ds = [d for d in ds if os.path.splitext(os.path.basename(d))[0] in only]
    res = {}
    for d in ds:
        n = os.path.splitext(os.path.basename(d))[0]
        res[n] = one(d, which)
        if verbose: print("%-28s %-18s %-20s %s" % (n, res[n][0], res[n][2], res[n][1]))
    import json
    json.dump(res, open(os.path.join(WORK, "result-%s.json" % which), "w"), indent=0)
    by = collections.Counter((r[2], r[0]) for r in res.values() if r[2])
    tiers = sorted({r[2] for r in res.values() if r[2]})
    print("as=%s  tests=%d  skipped-target=%d  harness(non-objdump/limits)=%d" % (which, len(res),
          sum(1 for r in res.values() if r[0] == "skipped-target"), sum(1 for r in res.values() if r[0].startswith("harness"))))
    for t in tiers:
        tot = sum(v for (tt, _), v in by.items() if tt == t)
        rest = ", ".join("%s %d" % (k, v) for (tt, k), v in sorted(by.items()) if tt == t and k != "pass")
        print("  %-20s %4d  pass %4d   %s" % (t, tot, by[(t, "pass")], rest))

if __name__ == "__main__":
    main()
