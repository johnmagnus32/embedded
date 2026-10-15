#!/usr/bin/env python3
"""
fetch_symbols.py — download KiCad symbol(s) + footprint(s) from JLCPCB/LCSC by part number.

Prereq: easyeda2kicad  (e.g.  .venv/bin/pip install easyeda2kicad)

Usage:
    python3 fetch_symbols.py C137955                     # one LCSC part
    python3 fetch_symbols.py C137955 C2480 C58756        # several
    python3 fetch_symbols.py C137955 --desc "5.1Ω 0402"  # + set a human ki_description (single part)

Appends to symbols/easyeda2kicad.kicad_sym (+ symbols/easyeda2kicad.pretty/). A part ALREADY in the
library is skipped — never re-downloaded or overwritten — so hand edits to existing symbols are kept.
There is no hardcoded parts list: pass whatever LCSC ids you need.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent / "symbols"
SYM_FILE = OUTPUT / "easyeda2kicad.kicad_sym"


def already_fetched(lcsc_id):
    """True if this LCSC id is already in the .kicad_sym library."""
    return SYM_FILE.exists() and lcsc_id in SYM_FILE.read_text(encoding="utf-8")


def fetch(lcsc_id):
    """Download one LCSC symbol+footprint unless already present (no --overwrite). Returns True if the
    part is in the library afterward (already there or newly fetched), False on a fetch failure."""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if already_fetched(lcsc_id):
        print(f"  {lcsc_id} — already present, skipping")
        return True
    # invoke via the current interpreter so it works whether or not the venv is activated
    r = subprocess.run(
        [sys.executable, "-m", "easyeda2kicad", f"--lcsc_id={lcsc_id}",
         "--symbol", "--footprint", "--output", str(OUTPUT)],
        capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  {lcsc_id} — FAILED: {(r.stderr or r.stdout).strip()}")
        return False
    print(f"  {lcsc_id} — fetched")
    return True


# --------------------------------------------------------------------------- #
# ki_description patch (balanced-paren, string-aware) — the human value/blurb that
# generate_schematic's disp_value reads for R/C/L display values.
# --------------------------------------------------------------------------- #
def _skip_string(text, i):
    i += 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
            continue
        if text[i] == '"':
            return i + 1
        i += 1
    return i


def _match_paren(text, open_idx):
    depth, i = 0, open_idx
    while i < len(text):
        c = text[i]
        if c == '"':
            i = _skip_string(text, i)
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def _find_property(block, name):
    """(start, end) of the `(property "name" ...)` s-expr in block, or None."""
    search = 0
    while True:
        p = block.find("(property", search)
        if p == -1:
            return None
        end = _match_paren(block, p)
        if end == -1:
            return None
        m = re.match(r'\(property\s+"([^"]*)"', block[p:end])
        if m and m.group(1) == name:
            return (p, end)
        search = end


def _make_desc_prop(desc, idnum):
    esc = desc.replace("\\", "\\\\").replace('"', '\\"')
    id_line = f"      (id {idnum})\n" if idnum is not None else ""
    return (f'    (property\n      "ki_description"\n      "{esc}"\n{id_line}'
            f"      (at 0 0 0)\n      (effects (font (size 1.27 1.27) ) hide)\n    )")


def patch_description(lcsc_id, desc):
    """Set/replace the ki_description of the symbol carrying `lcsc_id` (for value display etc)."""
    content = SYM_FILE.read_text(encoding="utf-8")
    idx = content.find(f'"{lcsc_id}"')
    if idx == -1:
        print(f"  (could not set --desc: {lcsc_id} not found in the library)")
        return
    sym_start = content.rfind('\n  (symbol "', 0, idx)
    next_sym = content.find('\n  (symbol "', idx)
    if next_sym == -1:
        next_sym = content.rfind("\n)")            # end of library
    block = content[sym_start:next_sym]
    existing = _find_property(block, "ki_description")
    if existing:
        start, end = existing
        m = re.search(r"\(id (\d+)\)", block[start:end])
        idnum = int(m.group(1)) if m else None
        new_block = block[:start] + _make_desc_prop(desc, idnum).lstrip() + block[end:]
    else:
        value = _find_property(block, "Value")
        if not value:
            print(f"  (could not set --desc: no Value property on {lcsc_id})")
            return
        ids = [int(n) for n in re.findall(r"\(id (\d+)\)", block)]
        idnum = (max(ids) + 1) if ids else None
        new_block = block[:value[1]] + "\n" + _make_desc_prop(desc, idnum) + block[value[1]:]
    if new_block != block:
        SYM_FILE.write_text(content[:sym_start] + new_block + content[next_sym:], encoding="utf-8")
        print(f"  {lcsc_id} — set ki_description")


def main():
    ap = argparse.ArgumentParser(description="Fetch KiCad symbol(s)+footprint(s) from LCSC by part number.")
    ap.add_argument("lcsc", nargs="+", metavar="LCSC_ID", help="LCSC part number(s), e.g. C137955")
    ap.add_argument("--desc", help="human ki_description to set (only valid with a single LCSC_ID)")
    args = ap.parse_args()
    if args.desc and len(args.lcsc) != 1:
        ap.error("--desc can only be used with a single LCSC_ID")

    print(f"Fetching from JLCPCB/LCSC into {OUTPUT} ...")
    ok = all(fetch(lcsc) for lcsc in args.lcsc)
    if args.desc and ok:
        patch_description(args.lcsc[0], args.desc)
    print(f"Done. Library: {SYM_FILE}")
    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
