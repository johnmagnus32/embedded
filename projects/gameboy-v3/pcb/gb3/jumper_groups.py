#!/usr/bin/env python3
"""jumper_groups.py — mark pads that are joined INSIDE the part, so DRC and routing need no copper tie between them.

  TS-1187A tactiles (SW1-SW11): A-B (pads 1, 2) and C-D (pads 3, 4) are the internally common pairs (BOM.md round 3).
  USBLC6-2SC6 (D4): pass-through, pins 1 & 6 are one internal node (I/O1), 3 & 4 the other (I/O2) (C7519.pdf p.1).

KiCad 9+ writes these as the footprint's (jumper_pad_groups ...); the KiCad 10 python binding cannot set them, so this
edits the board file text, board-level only (D4's SOT-23-6 library footprint is shared with U6, which must not get
them). Idempotent. Run with KiCad closed:  python3 jumper_groups.py [board.kicad_pcb]
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
GROUPS = {**{"SW%d" % i: (("1", "2"), ("3", "4")) for i in range(1, 12)}, "D4": (("1", "6"), ("3", "4"))}


def blocks(s):
    """(start, end, ref) of every top-level footprint block"""
    for m in re.finditer(r'^\t\(footprint "', s, re.M):
        i, d = m.start(), 0
        for j in range(i, len(s)):
            if s[j] == "(": d += 1
            elif s[j] == ")":
                d -= 1
                if d == 0: break
        r = re.search(r'\(property "Reference" "([^"]*)"', s[i:j])
        yield i, j + 1, r.group(1) if r else None


def main():
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    s = open(path).read()
    out, last, done, n = [], 0, set(), 0
    for i, j, ref in blocks(s):
        if ref not in GROUPS: continue
        blk = s[i:j]
        # KiCad's own serialisation (one group per line), so a KiCad save in between changes nothing
        want = "\t\t(jumper_pad_groups\n%s\t\t)\n" % "".join(
            "\t\t\t(%s)\n" % " ".join('"%s"' % p for p in g) for g in GROUPS[ref])
        k0 = blk.find("\t\t(jumper_pad_groups")
        if k0 >= 0:                                    # replace the whole (possibly multi-line) block
            d, k1 = 0, None
            for k in range(k0 + 2, len(blk)):
                if blk[k] == "(": d += 1
                elif blk[k] == ")":
                    d -= 1
                    if d == 0: k1 = k; break
            k1 = blk.find("\n", k1) + 1
            new = blk[:k0] + want + blk[k1:]
        else:
            k = blk.find("\t\t(duplicate_pad_numbers_are_jumpers")
            if k < 0: sys.exit("%s: no duplicate_pad_numbers_are_jumpers line to anchor on" % ref)
            k = blk.find("\n", k) + 1
            new = blk[:k] + want + blk[k:]
        n += new != blk
        out += [s[last:i], new]; last = j; done.add(ref)
    out.append(s[last:])
    missing = set(GROUPS) - done
    if missing: sys.exit("not on the board: %s" % ", ".join(sorted(missing)))
    open(path, "w").write("".join(out))
    print("jumper_groups: %d of %d footprints changed" % (n, len(GROUPS)))


if __name__ == "__main__":
    main()
