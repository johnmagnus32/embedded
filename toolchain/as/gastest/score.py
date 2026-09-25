#!/usr/bin/env python3
"""Score our as on the binutils gas/arm suite, counting ONLY tests the harness can validate (GNU as 2.42 passes
them through the same harness), grouped by feature tier. Run `run.py --as gnu` and `run.py --as ours` first."""
import json, collections, os
W = os.environ.get("GASTEST_WORK", "/tmp/gastest")
g = json.load(open(os.path.join(W, "result-gnu.json"))); o = json.load(open(os.path.join(W, "result-ours.json")))
g7p = os.path.join(W, "result-gnu7.json"); g7 = json.load(open(g7p)) if os.path.exists(g7p) else {}
valid = [k for k in o if g[k][0] == "pass"]
by = collections.defaultdict(collections.Counter)
for k in valid:
    t = o[k][2]
    # GNU as passes as-is but FAILS when forced to our target (ARMv7-A, ARM state): the test needs another architecture
    if g7 and g7[k][0] != "pass" and not t.startswith("diag"): t = "needs-other-arch"
    by[t][o[k][0]] += 1
print("scored tests (GNU as passes them through this harness): %d" % len(valid))
for t in sorted(by):
    c = by[t]; tot = sum(c.values())
    print("  %-20s %4d  pass %4d (%3d%%)   %s" % (t, tot, c["pass"], 100 * c["pass"] // tot, ", ".join("%s %d" % (k, v) for k, v in sorted(c.items()) if k != "pass")))
