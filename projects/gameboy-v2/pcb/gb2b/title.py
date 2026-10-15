#!/usr/bin/env python3
"""title.py — stamp the gb2 rev B title block into every gb2b schematic sheet and the board (idempotent).

cad/generate_schematic.py writes no title block, so a regeneration strips it: run this after every generator
run (and after anything else that rewrites the sheets). Re-running replaces the block; it never adds a second one.

Run:  python3 title.py            (from anywhere; edits gb2b.kicad_sch, sheets/gb2b-*.kicad_sch, gb2b.kicad_pcb)
Close KiCad first: it would overwrite the files on its next save.
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TITLE = "gb2 rev B"
DATE = "2026-10-14"          # bump when a revision of the design is issued
REV = "B"
COMMENTS = ["Rev A (../gb2/) is ordered and frozen; rev B = ER-TFT028A2-4 panel + Adafruit 258 cell",
            "Spec: ../REVB_DESIGN.md"]


def block(title, indent="\t"):
    i2 = indent * 2
    lines = [f"{indent}(title_block", f'{i2}(title "{title}")', f'{i2}(date "{DATE}")', f'{i2}(rev "{REV}")']
    lines += [f'{i2}(comment {n} "{c}")' for n, c in enumerate(COMMENTS, 1)]
    return "\n".join(lines) + f"\n{indent})\n"


def stamp(path, title):
    t = open(path, encoding="utf-8").read()
    t = re.sub(r"\t\(title_block\n.*?\n\t\)\n", "", t, count=1, flags=re.S)   # drop an existing block
    m = re.search(r"\n\t\(paper [^\n]*\)\n", t)
    if not m:
        sys.exit(f"ERROR: no (paper ...) line in {path}")
    t = t[:m.end()] + block(title) + t[m.end():]
    open(path, "w", encoding="utf-8").write(t)
    print(f"  {os.path.relpath(path, HERE)}: {title}")


def main():
    root = os.path.join(HERE, "gb2b.kicad_sch")
    stamp(root, TITLE)
    for p in sorted(glob.glob(os.path.join(HERE, "sheets", "gb2b-*.kicad_sch"))):
        stamp(p, f"{TITLE} - {os.path.basename(p)[len('gb2b-'):-len('.kicad_sch')]}")
    stamp(os.path.join(HERE, "gb2b.kicad_pcb"), TITLE)


if __name__ == "__main__":
    main()
