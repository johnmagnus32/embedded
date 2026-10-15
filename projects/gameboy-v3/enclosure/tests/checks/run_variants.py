#!/usr/bin/env python3
"""tests/checks/run_variants.py - checks.py under every one-flag variant (+ a combination), each in its own process
(params reads the GB3_* flags on import). Logs: build/checks/variants/<name>.log; prints each run's summary.

Run: .venv/bin/python tests/checks/run_variants.py [--fast] [name ...]   (names: the VARIANTS keys; default all)
"""
import os, subprocess, sys, time
ENCL = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LOGS = os.path.join(ENCL, "build", "checks", "variants")
VARIANTS = {
    "baseline": {},
    "lip": {"GB3_FRONT": "lip"},
    "hump": {"GB3_BACK": "hump"},
    "jauch": {"GB3_CELL": "LP503759JU"},
    "ada258": {"GB3_CELL": "ADA258"},
    "lcd_plate": {"GB3_LCD_PLATE": "1"},
    "fdm": {"GB3_PROCESS": "fdm"},
    "hump_ada258": {"GB3_BACK": "hump", "GB3_CELL": "ADA258"},
}

if __name__ == "__main__":
    os.makedirs(LOGS, exist_ok=True)
    names = [a for a in sys.argv[1:] if not a.startswith("--")] or list(VARIANTS)
    extra = ["--fast"] if "--fast" in sys.argv else []
    for n in names:
        env = dict(os.environ, GB3_QUIET="1", **VARIANTS[n])
        t0 = time.time()
        p = subprocess.run([os.path.join(ENCL, ".venv", "bin", "python"), "checks.py"] + extra, cwd=ENCL, env=env,
                           capture_output=True, text=True)
        log = os.path.join(LOGS, f"{n}.log")
        open(log, "w").write(p.stdout + p.stderr)
        lines = (p.stdout + p.stderr).splitlines()
        i = max((k for k, l in enumerate(lines) if l.startswith("summary")), default=None)
        print(f"== {n} ({' '.join(f'{k}={v}' for k, v in VARIANTS[n].items()) or 'defaults'}): exit {p.returncode}, "
              f"{time.time() - t0:.0f} s, log {os.path.relpath(log, ENCL)}")
        for l in (lines[i + 1:] if i is not None else lines[-15:]):
            print("   " + l[:400])
        sys.stdout.flush()
