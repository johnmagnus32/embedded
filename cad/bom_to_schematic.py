#!/usr/bin/env python3
"""
bom_to_schematic.py — Place all parts from a CSV into a KiCad schematic.

Reads a resolved parts CSV and writes <name>.kicad_sch with every part placed
on a grid (grouped by section), plus the project sym-/fp-lib-table. Nothing is
wired — see wire_schematic.py for connectivity.

CSV schema (one row per physical part):
    refdes     schematic reference (e.g. C1). The STABLE key nets.csv joins on.
               Blank placeable rows are auto-assigned the next free number for
               their prefix and written back, so refdes never depends on order.
    section    section heading -> schematic group + header text
    ref_num    provenance id (e.g. BOM #); NOT the refdes
    role       human role text (informational)
    part       manufacturer part / value text (informational)
    lcsc       LCSC code -> "LCSC" field on the symbol ("" if none)
    fit        JLC | Hand (informational)
    symbol     library id "nick:name" to place; "" or place=no -> skipped
    footprint  informational; the real def comes from the library
    place      yes | no
    note       free-form (informational)

A row is placed iff place=yes and its `symbol` resolves in the libraries.

Usage (--csv is required; --project defaults to gameboy-v3):
    python3 bom_to_schematic.py --csv <parts.csv>
    python3 bom_to_schematic.py --csv <parts.csv> --validate  # + kicad-cli sch erc
"""

import argparse
import csv as csvmod
import os
import re
import shutil
import subprocess
import sys
import tempfile
import uuid as uuidlib
from pathlib import Path

# --------------------------------------------------------------------------- #
# Defaults (relative to this file's location: <repo>/cad/)
# --------------------------------------------------------------------------- #
HERE = Path(__file__).resolve().parent
REPO = HERE.parent
DEFAULT_PROJECT = REPO / "projects/gameboy-v3/pcb/gb3"
DEFAULT_SYMBOLS = REPO / "cad/symbols"

LIBS = ["easyeda2kicad", "manual"]

# Layout
PITCH_X, PITCH_Y = 44.45, 38.1
COLS = 12
MARGIN = 25.4
HEADER_GAP = SECTION_GAP = 12.7


# --------------------------------------------------------------------------- #
# S-expression helpers (balanced-paren, string-aware)
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


def match_paren(text, open_idx):
    """text[open_idx] is '('; return index just past the matching ')', or -1."""
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


def find_kicad_cli():
    exe = shutil.which("kicad-cli")
    if exe:
        return exe
    mac = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
    if Path(mac).exists():
        return mac
    sys.exit("ERROR: kicad-cli not found (need KiCad installed).")


def canonicalize_sch(cli, sch_path):
    """Round-trip an emitted schematic through KiCad's own parser/formatter.

    `sch upgrade --force` re-saves it in place, so KiCad itself validates and
    canonicalizes our hand-written s-expr — the same trick load_symbols uses for
    symbol libs. Fails loud if KiCad can't parse what we wrote.
    """
    r = subprocess.run([cli, "sch", "upgrade", "--force", str(sch_path)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ERROR: KiCad rejected the generated schematic "
                 f"(sch upgrade failed):\n{r.stderr or r.stdout}")


# --------------------------------------------------------------------------- #
# Library parsing
# --------------------------------------------------------------------------- #
class Symbol:
    __slots__ = ("nick", "name", "block", "ref", "ref_found", "footprint",
                 "value", "datasheet", "description", "lcsc")

    def __init__(self, nick, name, block):
        self.nick, self.name, self.block = nick, name, block
        ref = self._prop("Reference")
        self.ref_found = ref is not None      # False -> refdes prefix is a guess
        self.ref = ref or "U"
        self.value = self._prop("Value") or name
        self.footprint = self._prop("Footprint") or ""
        self.datasheet = self._prop("Datasheet") or ""
        self.description = (self._prop("Description")
                            or self._prop("ki_description") or "")
        self.lcsc = self._prop("LCSC Part")

    def _prop(self, key):
        m = re.search(r'\(property\s+"%s"\s+"((?:[^"\\]|\\.)*)"' % re.escape(key),
                      self.block)
        return m.group(1).replace('\\"', '"').replace("\\\\", "\\") if m else None

    @property
    def lib_id(self):
        return f"{self.nick}:{self.name}"

    def embedded_def(self):
        return self.block.replace(f'(symbol "{self.name}"',
                                  f'(symbol "{self.lib_id}"', 1)


def load_symbols(symbols_dir, cli):
    """Upgrade throwaway copies of the libs to current format, parse symbols.

    Returns (by_lcsc, by_libid): {LCSC# -> Symbol}, {"nick:name" -> Symbol}.
    """
    tmp = Path(tempfile.mkdtemp(prefix="bom2sch_"))
    by_lcsc, by_libid = {}, {}
    try:
        for nick in LIBS:
            src = symbols_dir / f"{nick}.kicad_sym"
            if not src.exists():
                sys.exit(f"ERROR: symbol lib not found: {src}")
            dst = tmp / f"{nick}.kicad_sym"
            shutil.copy(src, dst)
            r = subprocess.run([cli, "sym", "upgrade", "--force", str(dst)],
                               capture_output=True, text=True)
            if r.returncode != 0:
                sys.exit(f"ERROR: sym upgrade failed for {nick}:\n{r.stderr}")
            txt = dst.read_text(encoding="utf-8")
            for m in re.finditer(r'\n\t\(symbol "([^"]+)"', txt):
                name = m.group(1)
                if re.search(r"_\d+_\d+$", name):
                    continue
                end = match_paren(txt, m.start() + 1)
                if end == -1:
                    continue
                sym = Symbol(nick, name, txt[m.start() + 1:end])
                by_libid[sym.lib_id] = sym
                if sym.lcsc and sym.lcsc not in by_lcsc:
                    by_lcsc[sym.lcsc] = sym
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not by_libid:
        sys.exit("ERROR: parsed 0 symbols from the libraries — the .kicad_sym "
                 "read-back likely broke (formatter/indentation changed).")
    return by_lcsc, by_libid


# --------------------------------------------------------------------------- #
# CSV read / write / refdes assignment
# --------------------------------------------------------------------------- #
CSV_COLUMNS = ["refdes", "section", "ref_num", "role", "part", "lcsc",
               "fit", "symbol", "footprint", "place", "note"]


def read_csv(path):
    resolved = []
    with open(path, newline="", encoding="utf-8") as f:
        rd = csvmod.DictReader(f)
        missing = {"refdes", "symbol", "place"} - set(rd.fieldnames or [])
        if missing:
            sys.exit(f"ERROR: {path} is missing required column(s): "
                     f"{', '.join(sorted(missing))} (header: {rd.fieldnames})")
        for r in rd:
            resolved.append(dict(
                refdes=r.get("refdes", "").strip(),
                section=r.get("section", "misc"), ref_num=r.get("ref_num", ""),
                role=r.get("role", ""),
                part=r.get("part", ""), lcsc=r.get("lcsc", ""),
                fit=r.get("fit", ""), symbol=r.get("symbol", "").strip(),
                footprint=r.get("footprint", ""),
                place=r.get("place", "").strip().lower() in ("yes", "true", "1"),
                note=r.get("note", ""),
            ))
    return resolved


def write_csv(resolved, path):
    """Persist resolved rows (used to write back auto-assigned refdes)."""
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csvmod.DictWriter(f, fieldnames=CSV_COLUMNS)
        w.writeheader()
        for r in resolved:
            row = dict(r)
            row["place"] = "yes" if r["place"] else "no"
            w.writerow({k: row.get(k, "") for k in CSV_COLUMNS})


def assign_refdes(resolved, by_libid):
    """Fill blank refdes for placeable rows; return count newly assigned.

    Stable identity: existing refdes are kept untouched; each blank gets the
    lowest free number for its prefix (the symbol's Reference prefix). Because
    rows that already have a refdes are never renumbered, reordering / adding /
    removing rows can't remap an existing part — so nets.csv (which keys on
    refdes) stays valid across placement changes.
    """
    used = {}
    for r in resolved:
        m = re.match(r"([A-Za-z]+)(\d+)$", r["refdes"])
        if m:
            used.setdefault(m.group(1), set()).add(int(m.group(2)))
    assigned = 0
    for r in resolved:
        if not r["place"] or r["refdes"]:
            continue
        sym = by_libid.get(r["symbol"])
        if not sym:
            continue
        seen = used.setdefault(sym.ref, set())
        n = 1
        while n in seen:
            n += 1
        seen.add(n)
        r["refdes"] = f"{sym.ref}{n}"
        assigned += 1
    return assigned


# --------------------------------------------------------------------------- #
# Stage 2: place resolved rows -> schematic
# --------------------------------------------------------------------------- #
def _u():
    return str(uuidlib.uuid4())


def _prop(name, value, x, y, hide=False):
    v = value.replace("\\", "\\\\").replace('"', '\\"')
    hide_line = "\t\t\t(hide yes)\n" if hide else ""
    return (f'\t\t(property "{name}" "{v}"\n\t\t\t(at {x:.2f} {y:.2f} 0)\n'
            f"{hide_line}\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)"
            f"\n\t\t\t\t)\n\t\t\t)\n\t\t)\n")


def _instance_symbol(sym, refdes, lcsc, x, y, root_uuid, project_name):
    props = _prop("Reference", refdes, x, y - 2.54)
    props += _prop("Value", sym.value, x, y + 2.54)
    props += _prop("Footprint", sym.footprint, x, y, hide=True)
    if sym.datasheet:
        props += _prop("Datasheet", sym.datasheet, x, y, hide=True)
    if sym.description:
        props += _prop("Description", sym.description, x, y, hide=True)
    if lcsc:
        props += _prop("LCSC", lcsc, x, y, hide=True)
    return (f"\t(symbol\n\t\t(lib_id \"{sym.lib_id}\")\n\t\t(at {x:.2f} {y:.2f} 0)"
            f"\n\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n"
            f"\t\t(on_board yes)\n\t\t(dnp no)\n\t\t(uuid \"{_u()}\")\n{props}"
            f"\t\t(instances\n\t\t\t(project \"{project_name}\"\n"
            f"\t\t\t\t(path \"/{root_uuid}\"\n\t\t\t\t\t(reference \"{refdes}\")\n"
            f"\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n")


def _text(label, x, y):
    v = label.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(text "{v}"\n\t\t(exclude_from_sim no)\n\t\t(at {x:.2f} {y:.2f} 0)'
            f"\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 2.5 2.5)\n\t\t\t\t(bold yes)"
            f"\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n\t\t(uuid \"{_u()}\")\n\t)\n")


def build_schematic(resolved, by_libid, project_name):
    """Place resolved rows (place=yes with a symbol + refdes), one per row.

    Refdes come straight from the CSV (see assign_refdes) — never recomputed
    here — so placement order does not affect them.

    Returns (sch_text, n_placed, n_defs, unresolved_placeables).
    """
    root_uuid = _u()

    # One placement per placeable row; refdes taken from the CSV.
    placements, unresolved = [], []
    for r in resolved:
        if not r["place"]:
            continue
        sym = by_libid.get(r["symbol"])
        if not sym or not r["refdes"]:
            unresolved.append(r)   # place=yes but symbol missing / refdes blank
            continue
        placements.append((sym, r["section"], r["lcsc"], r["refdes"]))

    # Group by section (first-seen order) and lay out.
    order, groups = [], {}
    for p in placements:
        groups.setdefault(p[1], []).append(p)
        if p[1] not in order:
            order.append(p[1])

    body, used = [], {}
    y, max_x = MARGIN, MARGIN
    for s in order:
        body.append(_text(s, MARGIN, y))
        y += HEADER_GAP
        col = 0
        for sym, _s, lcsc, refdes in groups[s]:
            x = MARGIN + col * PITCH_X
            max_x = max(max_x, x)
            body.append(_instance_symbol(sym, refdes, lcsc, x, y,
                                         root_uuid, project_name))
            used[sym.lib_id] = sym
            col += 1
            if col >= COLS:
                col, y = 0, y + PITCH_Y
        if col:
            y += PITCH_Y
        y += SECTION_GAP

    width = min(max(round(MARGIN + max_x + PITCH_X), 297), 1200)
    height = min(max(round(y + MARGIN), 210), 1200)
    lib_defs = "".join(f"\t\t{d.embedded_def()}\n" for d in used.values())

    sch = (f'(kicad_sch\n\t(version 20260306)\n\t(generator "bom_to_schematic")\n'
           f'\t(generator_version "10.0")\n\t(uuid "{root_uuid}")\n'
           f'\t(paper "User" {width} {height})\n\t(lib_symbols\n{lib_defs}\t)\n'
           f"{''.join(body)}\t(sheet_instances\n\t\t(path \"/\"\n\t\t\t(page \"1\")"
           f"\n\t\t)\n\t)\n\t(embedded_fonts no)\n)\n")
    return sch, len(placements), len(used), unresolved


# --------------------------------------------------------------------------- #
# Library tables
# --------------------------------------------------------------------------- #
def write_lib_tables(project_dir, symbols_dir):
    rel = os.path.relpath(symbols_dir, project_dir)

    def merge(fname, root_tag, entries):
        path = project_dir / fname
        if not path.exists():
            path.write_text(f"({root_tag}\n\t(version 7)\n" +
                            "\n".join(entries) + "\n)\n")
            return "created"
        txt, added = path.read_text(), False
        for e in entries:
            name = re.search(r'\(name "([^"]+)"\)', e).group(1)
            if f'(name "{name}")' in txt:
                continue
            txt = re.sub(r"\)\s*$", e + "\n)", txt, count=1)
            added = True
        path.write_text(txt)
        return "updated" if added else "unchanged"

    sym = [f'\t(lib (name "{n}")(type "KiCad")'
           f'(uri "${{KIPRJMOD}}/{rel}/{n}.kicad_sym")(options "")(descr ""))'
           for n in LIBS]
    fp = [f'\t(lib (name "{n}")(type "KiCad")'
          f'(uri "${{KIPRJMOD}}/{rel}/{n}.pretty")(options "")(descr ""))'
          for n in LIBS]
    return (merge("sym-lib-table", "sym_lib_table", sym),
            merge("fp-lib-table", "fp_lib_table", fp))


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project", type=Path, default=DEFAULT_PROJECT,
                    help="dir containing <name>.kicad_pro")
    ap.add_argument("--symbols", type=Path, default=DEFAULT_SYMBOLS)
    ap.add_argument("--csv", type=Path, required=True,
                    help="input parts CSV (required)")
    ap.add_argument("--validate", action="store_true",
                    help="run `kicad-cli sch erc` on the result")
    args = ap.parse_args()

    cli = find_kicad_cli()
    project_dir = args.project.resolve()
    pro = next(project_dir.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project_dir}")
    project_name = pro.stem
    csv_path = args.csv
    if not csv_path.exists():
        sys.exit(f"ERROR: CSV not found: {csv_path}")

    print(f"Loading symbol libraries from {args.symbols} (upgrading copies)...")
    by_lcsc, by_libid = load_symbols(args.symbols.resolve(), cli)
    print(f"  {len(by_lcsc)} symbols indexed by LCSC#, {len(by_libid)} total.")

    print(f"Reading parts from {csv_path}...")
    resolved = read_csv(csv_path)
    print(f"  {len(resolved)} CSV rows "
          f"({sum(1 for r in resolved if r['place'])} place).")

    n_new = assign_refdes(resolved, by_libid)
    if n_new:
        write_csv(resolved, csv_path)
        print(f"  assigned {n_new} new refdes, wrote them back to {csv_path.name}")

    # --- build schematic from resolved rows ---
    sch_text, n_placed, n_defs, unresolved = build_schematic(
        resolved, by_libid, project_name)
    sch_path = project_dir / f"{project_name}.kicad_sch"
    if n_placed == 0:
        sys.exit(f"ERROR: 0 placeable symbols from {csv_path.name} — refusing to "
                 f"overwrite {sch_path.name}. Check its symbol/place columns.")
    sch_path.write_text(sch_text, encoding="utf-8")
    print(f"Wrote {sch_path}")
    print(f"  {n_placed} symbols placed, {n_defs} unique definitions embedded.")
    canonicalize_sch(cli, sch_path)
    print("  validated + canonicalized via kicad-cli sch upgrade")

    missing = [(r["refdes"], r["symbol"],
                "no Reference" if not s.ref_found else "no Footprint")
               for r in resolved if r["place"]
               and (s := by_libid.get(r["symbol"])) and (not s.ref_found or not s.footprint)]
    if missing:
        print(f"  WARNING: {len(missing)} placed symbol(s) missing a key property "
              "(refdes prefix guessed / footprint blank):")
        for refdes, sym, what in missing[:10]:
            print(f"    {refdes} ({sym}): {what}")
    if unresolved:
        print(f"  WARNING: {len(unresolved)} CSV rows marked place=yes but their "
              f"symbol is not in the library:")
        for r in unresolved:
            print(f"    #{r['ref_num']} {r['role']}  symbol={r['symbol']!r}")

    s1, s2 = write_lib_tables(project_dir, args.symbols.resolve())
    print(f"  sym-lib-table: {s1}, fp-lib-table: {s2}")

    if args.validate:
        print("Validating with kicad-cli sch erc...")
        erc_rpt = sch_path.with_name(f"{project_name}-erc.rpt")
        r = subprocess.run([cli, "sch", "erc", "--exit-code-violations",
                            "-o", str(erc_rpt), str(sch_path)],
                           capture_output=True, text=True)
        print("  " + (r.stdout.strip() or r.stderr.strip()).replace("\n", "\n  "))
        print(f"  report: {erc_rpt}")
        print(f"  (erc exit {r.returncode}; nonzero = ERC violations, expected "
              "for an unwired bring-in)")


if __name__ == "__main__":
    main()
