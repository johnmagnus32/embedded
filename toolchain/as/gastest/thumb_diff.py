#!/usr/bin/env python3
"""thumb_diff.py — our Thumb encoder against GNU as, one instruction at a time, over a generated operand space:
every mnemonic we implement x registers (low, high, sp, lr, pc) x immediates at each field's edges (0, the max,
max+1, misaligned, negative). GNU as decides; for each line:
  GNU emits 16 bits   -> we must emit the identical halfword
  GNU emits 32 bits   -> we must refuse it (Thumb-2 wide encodings aren't implemented) — never emit something else
  GNU rejects it      -> we must reject it too
Usage: GNU=<cross prefix> AS=<our as> thumb_diff.py [-v]"""
import os, subprocess, sys, tempfile, itertools
from concurrent.futures import ThreadPoolExecutor

GNU = os.environ["GNU"]; AS = os.environ["AS"]; VERBOSE = "-v" in sys.argv
LO = ["r0", "r3", "r7"]; HI = ["r8", "r12", "sp", "lr", "pc"]; REGS = LO + HI
IMMS = [0, 1, 3, 4, 7, 8, 31, 32, 33, 62, 63, 64, 124, 125, 128, 252, 255, 256, 508, 510, 512, 1020, 1022, 1024, -1, -4, -0]

def gen():
    L = []
    for m in ["adds", "subs", "add", "sub"]:
        for a, b, c in itertools.product(REGS, REGS, REGS): L.append(f"{m} {a}, {b}, {c}")
        for a, b in itertools.product(REGS, REGS): L.append(f"{m} {a}, {b}")
        for a, b, i in itertools.product(REGS, REGS, IMMS): L.append(f"{m} {a}, {b}, #{i}")
        for a, i in itertools.product(REGS, IMMS): L.append(f"{m} {a}, #{i}")
        L.append(f"{m} r0, r1, r2, lsl #2")
    for m in ["ands", "eors", "adcs", "orrs", "sbcs", "bics", "and", "orr"]:
        for a, b, c in itertools.product(LO + ["r8"], LO + ["r8"], LO + ["r8"]): L.append(f"{m} {a}, {b}, {c}")
        for a, b in itertools.product(LO + ["r8"], LO + ["r8"]): L.append(f"{m} {a}, {b}")
        L.append(f"{m} r0, r0, #1")
    for m in ["lsls", "lsrs", "asrs", "rors", "lsl"]:
        for a, b, i in itertools.product(LO + ["r8"], LO + ["r8"], [0, 1, 5, 31, 32, 33]): L.append(f"{m} {a}, {b}, #{i}")
        for a, b in itertools.product(LO + ["r8"], LO + ["r8"]): L.append(f"{m} {a}, {b}")
        for a, b, c in itertools.product(LO, LO, LO): L.append(f"{m} {a}, {b}, {c}")
        for a, i in itertools.product(LO, [0, 1, 31, 32]): L.append(f"{m} {a}, #{i}")
    for m in ["movs", "mov"]:
        for a, b in itertools.product(REGS, REGS): L.append(f"{m} {a}, {b}")
        for a, i in itertools.product(REGS, IMMS): L.append(f"{m} {a}, #{i}")
        for a, b, s, i in itertools.product(LO + ["r8"], LO, ["lsl", "lsr", "asr", "ror"], [0, 1, 32]): L.append(f"{m} {a}, {b}, {s} #{i}")
        for a, s, c in itertools.product(LO, ["lsl", "lsr", "asr", "ror"], LO): L.append(f"{m} {a}, {a}, {s} {c}")
        L.append(f"{m} r0, r1, {'lsl'} r2")
    for m in ["cmp", "cmn", "tst"]:
        for a, b in itertools.product(REGS, REGS): L.append(f"{m} {a}, {b}")
        for a, i in itertools.product(REGS, IMMS): L.append(f"{m} {a}, #{i}")
    for m in ["mvns", "negs", "mvn", "neg"]:
        for a, b in itertools.product(LO + ["r8"], LO + ["r8"]): L.append(f"{m} {a}, {b}")
    for m in ["muls", "mul"]:
        for a, b, c in itertools.product(LO + ["r8"], LO, LO): L.append(f"{m} {a}, {b}, {c}")
        for a, b in itertools.product(LO, LO): L.append(f"{m} {a}, {b}")
    for m in ["rsbs", "rsb"]:
        for a, b, i in itertools.product(LO + ["r8"], LO, [0, 1]): L.append(f"{m} {a}, {b}, #{i}")
    for m in ["ldr", "str", "ldrb", "strb", "ldrh", "strh", "ldrsb", "ldrsh"]:
        for t, n in itertools.product(LO + ["r8"], LO + ["r8", "sp", "pc"]):
            L.append(f"{m} {t}, [{n}]")
            for i in [0, 1, 2, 4, 6, 31, 32, 62, 64, 124, 126, 128, 1020, 1024, -4]: L.append(f"{m} {t}, [{n}, #{i}]")
            for r in LO + ["r8"]: L.append(f"{m} {t}, [{n}, {r}]")
            L.append(f"{m} {t}, [{n}, -r1]"); L.append(f"{m} {t}, [{n}, #4]!"); L.append(f"{m} {t}, [{n}], #4"); L.append(f"{m} {t}, [{n}, r1, lsl #2]")
    lists = ["{r0}", "{r0, r1}", "{r0-r7}", "{r3, r7}", "{r0, lr}", "{r0, pc}", "{lr}", "{pc}", "{r8}", "{r0, r8}", "{r4-r7, lr}", "{r0, sp}"]
    for m in ["push", "pop"]:
        for l in lists: L.append(f"{m} {l}")
    for m in ["ldm", "ldmia", "ldmfd", "stm", "stmia", "stmea", "stmdb", "stmfd", "ldmdb", "ldmea"]:
        for n, wb, l in itertools.product(["r0", "r3", "r7", "r8", "sp"], ["", "!"], lists + ["{r1, r3}", "{r2-r4}"]): L.append(f"{m} {n}{wb}, {l}")
    for m in ["bx", "blx"]:
        for r in REGS: L.append(f"{m} {r}")
    for m in ["svc", "swi", "bkpt", "udf"]:
        for i in [0, 1, 255, 256, 65535]: L.append(f"{m} #{i}")
    L += ["bkpt", "udf", "nop", "yield", "wfe", "wfi", "sev", "hint #0", "hint #3", "hint #15", "hint #16"]
    for m in ["sxth", "sxtb", "uxth", "uxtb", "rev", "rev16", "revsh"]:
        for a, b in itertools.product(LO + ["r8"], LO + ["r8"]): L.append(f"{m} {a}, {b}")
        L.append(f"{m} r0, r1, ror #0"); L.append(f"{m} r0, r1, ror #8")
    for m in ["cpsie", "cpsid"]:
        for f in ["i", "f", "a", "if", "aif", "i, #16"]: L.append(f"{m} {f}")
    L += ["setend be", "setend le", "movs.n r0, #1", "adds.n r0, r1, r2", "add.n r0, r1, r2", "movs r0, 7", "adds r0, r0, 1"]
    return L

def assemble(tool, line):
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, "t.s"); o = os.path.join(d, "t.o"); b = os.path.join(d, "t.bin")
        open(s, "w").write(".syntax unified\n.thumb\n" + line + "\n")
        cmd = [GNU + "as", "-march=armv7-a", "-o", o, s] if tool == "gnu" else [AS, "-march=armv7-a", "-o", o, s]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(o): return None, (r.stderr or r.stdout).strip().splitlines()[-1:] or [""]
        subprocess.run([GNU + "objcopy", "-O", "binary", "-j", ".text", o, b], check=True)
        return open(b, "rb").read(), None

# Deliberate divergences: lines GNU as encodes as something other than what was written. We refuse them.
REFUSED = lambda line: any(line.startswith(m + " sp!,") for m in ("stm", "stmia", "stmea"))

def check(line):
    g, gerr = assemble("gnu", line); o, oerr = assemble("ours", line)
    if REFUSED(line) and o is None: return ("refused", line, g.hex() if g else gerr, oerr)
    if g is None: return ("pass" if o is None else "FAIL-accepts", line, gerr, o)
    if len(g) == 4: return ("wide" if o is None else ("pass" if o == g else "FAIL-bytes"), line, g.hex(), o.hex() if o else oerr)
    if o is None: return ("FAIL-rejects", line, g.hex(), oerr)
    return ("pass" if o == g else "FAIL-bytes", line, g.hex(), o.hex())

lines = gen()
with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex: res = list(ex.map(check, lines))
from collections import Counter
c = Counter(r[0] for r in res)
for r in res:
    if r[0].startswith("FAIL") or (VERBOSE and r[0] == "wide"): print(r[0], "|", r[1], "| gnu:", r[2], "| ours:", r[3])
print(f"thumb-diff: {len(lines)} lines — {c['pass']} identical/both-rejected, {c['wide']} 32-bit in GNU (refused), "
      f"{c['refused']} GNU mis-encodes (refused), "
      f"{sum(v for k, v in c.items() if k.startswith('FAIL'))} FAIL ({', '.join(f'{k} {v}' for k, v in c.items() if k.startswith('FAIL')) or 'none'})")
sys.exit(1 if any(k.startswith("FAIL") for k in c) else 0)
