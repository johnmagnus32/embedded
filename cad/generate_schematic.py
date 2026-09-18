#!/usr/bin/env python3
"""
generate_schematic.py — one command to build the whole KiCad schematic from the two source-of-truth CSVs.

It runs the two stages (formerly the separate scripts bom_to_schematic.py and wire_schematic.py, now
inlined below), in order:

    --bom  parts CSV  --place-->  every part on the sheet   (the PLACE stage: _place_stage)
    --nets nets CSV   --wire--->  net-label connectivity     (the WIRE  stage: _wire_stage)

so `generate_schematic.py --bom gb3-bom.csv --nets gb3-nets.csv` regenerates the placed + wired schematic
in one shot and then GATES on validation. This tool's ONLY job is BOM + nets -> schematic: it never invents
or bootstraps connectivity — the --nets CSV is the source of truth (a fresh skeleton, if ever needed for a
new board, is a separate concern, not this tool's).

After building, a mandatory validation gate runs (see validate_full) and the command FAILS unless: every
pin has a net (no TODO_* in the nets CSV; a blank net is an intentional no-connect), ERC has zero errors
and zero unconnected pins, and every rendered net actually landed on its pin.

Usage:
    python3 generate_schematic.py --bom gb3-bom.csv --nets gb3-nets.csv        # place + wire + validate
    python3 generate_schematic.py --bom .. --nets .. --only microsd display    # just these subcircuits
"""
from __future__ import annotations

import argparse
import csv as csvmod
import json
import math
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

# Layout (bbox shelf-packing; all mm)
MARGIN = 25.4
HEADER_GAP = SECTION_GAP = 12.7
GAP = 10.16          # clearance reserved around each symbol (pins + net labels)
SHEET_W = 1100.0     # wrap symbols to a new shelf past this width
MIN_BAND_H = 63.5    # reserved band height per subcircuit — room for a relayout
HAND_LAID = {"core-buck", "buck-boost", "charger", "usb-c", "amp"}   # subcircuits re-laid by a relayout script
SHEET_DIR = "sheets"   # child .kicad_sch files live in <project>/sheets/ to keep the root dir tidy


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
                 "value", "datasheet", "description", "lcsc",
                 "lminx", "lmaxx", "lminy", "lmaxy", "dlminy", "dlmaxy", "dlminx", "dlmaxx")

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
        self._bbox()

    def _bbox(self):
        """Lib-space extent (pin tips + ALL body graphics) — for overlap-free
        layout and label placement. Must capture polylines/arcs/circles, not just
        rectangles: many symbols (e.g. easyeda caps) draw their body as polylines,
        so a rectangle-only scan returned a zero-height box and put labels on the
        body. Mirror sch_lint.lib_points so bh matches the lint's body_bbox."""
        xs, ys, dxs, dys = [], [], [], []     # d* = DRAWN graphics only (no pins)
        for m in re.finditer(r"\(pin\s+\w+\s+\w+\s*\(at (-?[\d.]+) (-?[\d.]+)",
                             self.block):
            xs.append(float(m.group(1))); ys.append(float(m.group(2)))
        for m in re.finditer(r"\((?:start|end|mid|xy) (-?[\d.]+) (-?[\d.]+)\)",
                             self.block):
            xs.append(float(m.group(1))); ys.append(float(m.group(2)))
            dxs.append(float(m.group(1))); dys.append(float(m.group(2)))
        for m in re.finditer(r"\(center (-?[\d.]+) (-?[\d.]+)\)\s*\(radius ([\d.]+)\)",
                             self.block):
            cx, cy, r = float(m.group(1)), float(m.group(2)), float(m.group(3))
            xs += [cx - r, cx + r]; ys += [cy - r, cy + r]
            dxs += [cx - r, cx + r]; dys += [cy - r, cy + r]
        if not xs:                            # degenerate: assume a small part
            xs, ys = [-2.54, 2.54], [-2.54, 2.54]
        self.lminx, self.lmaxx = min(xs), max(xs)
        self.lminy, self.lmaxy = min(ys), max(ys)
        # drawn-body extent (the printed box, pins excluded) — for refdes/value placement
        # on ASYMMETRIC bodies: a symmetric Y offset floats a label off on the short side,
        # and an origin that isn't the box X-centre leaves ref/value off-centre.
        self.dlminy, self.dlmaxy = (min(dys), max(dys)) if dys else (self.lminy, self.lmaxy)
        self.dlminx, self.dlmaxx = (min(dxs), max(dxs)) if dxs else (self.lminx, self.lmaxx)

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
CSV_COLUMNS = ["refdes", "subcircuit", "section", "lcsc", "fit", "symbol",
               "footprint", "place", "note"]


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
                subcircuit=r.get("subcircuit", "").strip(),
                section=r.get("section", "misc"),
                lcsc=r.get("lcsc", ""),
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


def _prop(name, value, x, y, hide=False, just=None, ang=0):
    v = value.replace("\\", "\\\\").replace('"', '\\"')
    hide_line = "\t\t\t(hide yes)\n" if hide else ""
    just_line = f"\t\t\t\t(justify {just})\n" if just else ""
    return (f'\t\t(property "{name}" "{v}"\n\t\t\t(at {x:.2f} {y:.2f} {ang})\n'
            f"{hide_line}\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)"
            f"\n\t\t\t\t)\n{just_line}\t\t\t)\n\t\t)\n")


def horiz_text_ang(rot):
    """Field angle (symbol-local) that renders text HORIZONTAL and UPRIGHT for a
    symbol placed at `rot`. KiCad rotates field text with a 90/270 symbol rotation
    (compensate with 360-rot) but does NOT flip text for a 180 rotation — so 0 and
    180 both want field ang 0. Returning 180 for rot 180 renders the text upside
    down (the "GND letters are upside down on the up-pointing ground" bug)."""
    return (360 - rot) % 360 if rot in (90, 270) else 0


def ref_value_props(refdes, value, x, y, rot, side="left", bh=0.0, off=None,
                    body_y=None, body_x=None):
    """Reference+Value text placement, rotation-aware and always HORIZONTAL.

    Horizontal parts (rot 0/180): Reference stacked ABOVE, Value BELOW, placed
    OUTSIDE the drawn box so they clear the pin names inside it.
    Vertical parts (rot 90/270): stacked to one SIDE, hugging the part
    (justified toward it) so the text stays clear of the vertical wires that
    enter a rotated part's top/bottom. `side` picks left (default) or right.
    The field angle compensates for the symbol rotation (horiz_text_ang) so the
    rendered text is horizontal regardless of how the part is turned.

    `bh` = the symbol's lib-Y half-extent (max(|lminy|,|lmaxy|)); it becomes the
    body's sheet half-height for rot 0/180 and its sheet half-width for 90/270.
    `body_y` = (lib_min_y, lib_max_y) of the DRAWN box (pins excluded). When given,
    the rot-0/180 refdes/value clear the box PER SIDE — an ASYMMETRIC body (origin
    not at box centre, e.g. a chip with a top-only power pin) no longer floats the
    refdes far off on its short side (which then needed a manual per-part override).
    """
    if rot in (90, 270):
        off = off if off is not None else max(bh + 1.27, 3.81)   # clear the body to the side
        sx = x + (off if side == "right" else -off)
        just = "left" if side == "right" else "right"
        ta = horiz_text_ang(rot)
        return (_prop("Reference", refdes, sx, y - 1.27, just=just, ang=ta)   # tight stack
                + _prop("Value", value, sx, y + 1.27, just=just, ang=ta))
    # rot 0/180: clear the drawn box per side. In lib frame +y is UP; sheet-up maps
    # to lib +y at rot 0 and to lib -y at rot 180. Fall back to symmetric bh.
    if body_y is not None:
        lo, hi = body_y
        up, dn = (hi, -lo) if rot == 0 else (-lo, hi)
        above, below = max(up + 2.54, 2.54), max(dn + 2.54, 2.54)
    else:
        above = below = max(bh + 2.54, 2.54)
    # centre ref/value on the DRAWN box's X (the origin may not be the box centre, e.g. a
    # connector with all pins on one side) so they sit horizontally centred on the part.
    cx = x
    if body_x is not None:
        c = (body_x[0] + body_x[1]) / 2.0
        cx = x + (c if rot == 0 else -c)
    return (_prop("Reference", refdes, cx, y - above)
            + _prop("Value", value, cx, y + below))


# value+unit anywhere in a description, e.g. "100nF", "2.2uH", "75KΩ", "10mΩ",
# "240Ω", "0Ω" — the trailing lookahead rejects MPN fragments like "25F" in a P/N.
_VAL_RE = re.compile(
    r"(\d+(?:\.\d+)?)\s?(pF|nF|µF|uF|pH|nH|µH|uH|mH|mΩ|[kKMG]?Ω)(?![A-Za-z0-9])")


def disp_value(sym):
    """Concise human value (10k, 100nF, 2.2µH) for R/C/L; MPN for everything else."""
    if sym.ref not in ("R", "C", "L"):
        return sym.value
    m = _VAL_RE.search(sym.description or "")
    if not m:
        return sym.value
    num, unit = m.group(1), m.group(2)
    unit = {"uF": "µF", "uH": "µH", "KΩ": "k", "kΩ": "k",
            "MΩ": "M", "GΩ": "G"}.get(unit, unit)
    return f"{num}{unit}"


def _instance_symbol(sym, refdes, lcsc, x, y, inst_path, project_name):
    """One placed symbol. `inst_path` is the hierarchical instance path:
    "/<root>" for a flat sheet, or "/<root>/<sheet>" for a child sheet."""
    props = ref_value_props(refdes, disp_value(sym), x, y, 0,
                            bh=max(abs(sym.lminy), abs(sym.lmaxy)),
                            body_y=(sym.dlminy, sym.dlmaxy),
                            body_x=(sym.dlminx, sym.dlmaxx))
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
            f"\t\t\t\t(path \"{inst_path}\"\n\t\t\t\t\t(reference \"{refdes}\")\n"
            f"\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n")


# --------------------------------------------------------------------------- #
# Hierarchical sheets: deterministic UUIDs so regeneration keeps stable
# instance paths (netlist provenance) and clean git diffs.
# --------------------------------------------------------------------------- #
_UUID_NS = uuidlib.UUID("6ba7b811-9dad-11d1-80b4-00c04fd430c8")


def _det_uuid(*parts):
    return str(uuidlib.uuid5(_UUID_NS, ":".join(parts)))


def sheet_slug(grp):
    """Filesystem-safe child-sheet slug for a subcircuit group name."""
    return re.sub(r"[^A-Za-z0-9]+", "-", grp).strip("-").lower() or "misc"


def _sheet_block(name, fname, sheet_uuid, x, y, w, h, page, root_uuid, project_name):
    """A (sheet ...) instance on the root, pointing to a child .kicad_sch."""
    return (
        f"\t(sheet\n\t\t(at {x:.2f} {y:.2f})\n\t\t(size {w:.2f} {h:.2f})\n"
        f"\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n"
        f"\t\t(fields_autoplaced yes)\n\t\t(stroke\n\t\t\t(width 0.1524)\n\t\t\t(type solid)\n\t\t)\n"
        f"\t\t(fill\n\t\t\t(color 0 0 0 0.0000)\n\t\t)\n\t\t(uuid \"{sheet_uuid}\")\n"
        f'\t\t(property "Sheetname" "{name}"\n\t\t\t(at {x:.2f} {y - 0.7112:.2f} 0)\n'
        f"\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t)\n"
        f"\t\t\t\t(justify left bottom)\n\t\t\t)\n\t\t)\n"
        f'\t\t(property "Sheetfile" "{fname}"\n\t\t\t(at {x:.2f} {y + h + 0.5588:.2f} 0)\n'
        f"\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)\n\t\t\t\t)\n"
        f"\t\t\t\t(justify left top)\n\t\t\t)\n\t\t)\n"
        f'\t\t(instances\n\t\t\t(project "{project_name}"\n\t\t\t\t(path "/{root_uuid}"\n'
        f'\t\t\t\t\t(page "{page}")\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n')


def _text(label, x, y):
    v = label.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(text "{v}"\n\t\t(exclude_from_sim no)\n\t\t(at {x:.2f} {y:.2f} 0)'
            f"\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 2.5 2.5)\n\t\t\t\t(bold yes)"
            f"\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n\t\t(uuid \"{_u()}\")\n\t)\n")


def _pack_group(placements, inst_path, project_name):
    """Shelf-pack one group's symbols onto its own child canvas (origin at MARGIN).
    Returns (body_text, used{lib_id:Symbol}, width, height)."""
    body, used = [], {}
    shelf_x, shelf_top, shelf_h, max_x = MARGIN, MARGIN + HEADER_GAP, 0.0, MARGIN
    for sym, _grp, lcsc, refdes in placements:
        w = (sym.lmaxx - sym.lminx) + GAP
        h = (sym.lmaxy - sym.lminy) + GAP
        if shelf_x > MARGIN and shelf_x + w > SHEET_W:      # wrap to next shelf
            shelf_top += shelf_h + GAP
            shelf_x, shelf_h = MARGIN, 0.0
        ox = round((shelf_x + GAP / 2 - sym.lminx) / 1.27) * 1.27   # snap 50-mil
        oy = round((shelf_top + GAP / 2 + sym.lmaxy) / 1.27) * 1.27
        body.append(_instance_symbol(sym, refdes, lcsc, ox, oy, inst_path, project_name))
        used[sym.lib_id] = sym
        max_x = max(max_x, shelf_x + w)
        shelf_x += w
        shelf_h = max(shelf_h, h)
    width = min(max(round(max_x + MARGIN), 297), 5000)
    height = min(max(round(shelf_top + shelf_h + MARGIN), 210), 5000)
    return "".join(body), used, width, height


def build_schematic(resolved, by_libid, project_name):
    """Build a HIERARCHICAL schematic: a root sheet with one child sheet per
    subcircuit, each child holding that subcircuit's placed symbols.

    Putting each subcircuit on its own page makes per-subcircuit analysis robust
    (a tool opens exactly the child file — no spatial region can bleed a neighbour
    in or clip a member out). Cross-sheet connectivity is by GLOBAL labels + power
    symbols (added by wire_schematic / the relayout scripts).

    Refdes come straight from the CSV (assign_refdes) — never recomputed here.

    Returns (root_text, {child_fname: child_text}, n_placed, n_defs, unresolved).
    """
    root_uuid = _det_uuid(project_name, "root")

    placements, unresolved = [], []
    for r in resolved:
        if not r["place"]:
            continue
        sym = by_libid.get(r["symbol"])
        if not sym or not r["refdes"]:
            unresolved.append(r)   # place=yes but symbol missing / refdes blank
            continue
        grp = r.get("subcircuit") or r["section"] or "misc"
        placements.append((sym, grp, r["lcsc"], r["refdes"]))

    order, groups = [], {}
    for p in placements:
        groups.setdefault(p[1], []).append(p)
        if p[1] not in order:
            order.append(p[1])

    # ---- one child sheet file per group ----
    children, sheets, used_total = {}, [], {}
    for grp in order:
        sheet_uuid = _det_uuid(project_name, "sheet", grp)
        file_uuid = _det_uuid(project_name, "file", grp)
        inst_path = f"/{root_uuid}/{sheet_uuid}"
        cbody, cused, cw, ch = _pack_group(groups[grp], inst_path, project_name)
        used_total.update(cused)
        clib = "".join(f"\t\t{d.embedded_def()}\n" for d in cused.values())
        ctext = (f'(kicad_sch\n\t(version 20260306)\n\t(generator "bom_to_schematic")\n'
                 f'\t(generator_version "10.0")\n\t(uuid "{file_uuid}")\n'
                 f'\t(paper "User" {cw} {ch})\n\t(lib_symbols\n{clib}\t)\n'
                 f"{_text(grp, MARGIN, MARGIN)}{cbody}\t(embedded_fonts no)\n)\n")
        fname = f"{SHEET_DIR}/{project_name}-{sheet_slug(grp)}.kicad_sch"   # in the sheets/ subfolder
        children[fname] = ctext
        sheets.append((grp, fname, sheet_uuid))

    # ---- root: a grid of sheet blocks, one per child ----
    COLS = 6
    BOXW, BOXH, GX, GY = 45.0, 30.0, 20.0, 25.0
    rbody = []
    for i, (name, fname, suid) in enumerate(sheets):
        col, rowi = i % COLS, i // COLS
        x = MARGIN + col * (BOXW + GX)
        yy = MARGIN + HEADER_GAP + rowi * (BOXH + GY)
        rbody.append(_sheet_block(name, fname, suid, x, yy, BOXW, BOXH,
                                  i + 2, root_uuid, project_name))
    nrows = (len(sheets) + COLS - 1) // COLS
    rw = min(max(round(2 * MARGIN + COLS * (BOXW + GX)), 297), 5000)
    rh = min(max(round(2 * MARGIN + HEADER_GAP + nrows * (BOXH + GY)), 210), 5000)
    root = (f'(kicad_sch\n\t(version 20260306)\n\t(generator "bom_to_schematic")\n'
            f'\t(generator_version "10.0")\n\t(uuid "{root_uuid}")\n'
            f'\t(paper "User" {rw} {rh})\n\t(lib_symbols\n\t)\n'
            f"{''.join(rbody)}\t(sheet_instances\n\t\t(path \"/\"\n\t\t\t(page \"1\")"
            f"\n\t\t)\n\t)\n\t(embedded_fonts no)\n)\n")
    return root, children, len(placements), len(used_total), unresolved, sheets, root_uuid


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
def _place_stage(project, symbols, csv_path, only):
    """PLACE stage: put every part from the parts CSV on the schematic (grouped by subcircuit).

    `only` (list|None) regenerates ONLY those subcircuit child sheets, leaving the root, the .kicad_pro
    page list, and every other child sheet untouched — safe because sheet/file UUIDs are deterministic
    (uuid5), so a regenerated child still matches the existing root; it refuses if the sheet SET changed.
    """
    cli = find_kicad_cli()
    project_dir = project.resolve()
    pro = next(project_dir.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project_dir}")
    project_name = pro.stem
    if not csv_path.exists():
        sys.exit(f"ERROR: CSV not found: {csv_path}")

    print(f"Loading symbol libraries from {symbols} (upgrading copies)...")
    by_lcsc, by_libid = load_symbols(symbols.resolve(), cli)
    print(f"  {len(by_lcsc)} symbols indexed by LCSC#, {len(by_libid)} total.")

    print(f"Reading parts from {csv_path}...")
    resolved = read_csv(csv_path)
    print(f"  {len(resolved)} CSV rows "
          f"({sum(1 for r in resolved if r['place'])} place).")

    n_new = assign_refdes(resolved, by_libid)
    if n_new:
        write_csv(resolved, csv_path)
        print(f"  assigned {n_new} new refdes, wrote them back to {csv_path.name}")

    # --- build HIERARCHICAL schematic (root + one child sheet per subcircuit) ---
    root_text, children, n_placed, n_defs, unresolved, sheets, root_uuid = build_schematic(
        resolved, by_libid, project_name)
    sch_path = project_dir / f"{project_name}.kicad_sch"
    if n_placed == 0:
        sys.exit(f"ERROR: 0 placeable symbols from {csv_path.name} — refusing to "
                 f"overwrite {sch_path.name}. Check its symbol/place columns.")

    # --- selective regen: write ONLY the named child sheets, leave root + others alone ---
    if only:
        grp_to_fname = {grp: fname for grp, fname, _ in sheets}
        unknown = [g for g in only if g not in grp_to_fname]
        if unknown:
            sys.exit(f"ERROR: --only names are not subcircuits in {csv_path.name}: {unknown}\n"
                     f"  available: {', '.join(sorted(grp_to_fname))}")
        # deterministic UUIDs keep the existing root valid ONLY if the sheet SET is unchanged.
        (project_dir / SHEET_DIR).mkdir(exist_ok=True)
        existing = {p.resolve() for p in
                    list(project_dir.glob(f"{project_name}-*.kicad_sch"))
                    + list((project_dir / SHEET_DIR).glob(f"{project_name}-*.kicad_sch"))}
        computed = {(project_dir / f).resolve() for f in children}
        if existing != computed:
            sys.exit("ERROR: --only cannot change the SET of child sheets (that would need the "
                     "root + page list rebuilt).\n"
                     f"  would add:    {sorted(p.name for p in computed - existing)}\n"
                     f"  would remove: {sorted(p.name for p in existing - computed)}\n"
                     "  Run a full bootstrap (omit --only) instead.")
        for grp in only:
            fp = project_dir / grp_to_fname[grp]
            fp.write_text(children[grp_to_fname[grp]], encoding="utf-8")
            canonicalize_sch(cli, fp)
        print(f"Regenerated {len(only)} child sheet(s) [{', '.join(only)}] — "
              f"root, {pro.name} page list, and other sheets left untouched.")
        if unresolved:
            print(f"  WARNING: {len(unresolved)} place=yes row(s) have no library symbol.")
        return

    # Prune stale child sheets: any {project}-*.kicad_sch not in this run, in BOTH the
    # sheets/ subfolder AND the project root (older layouts kept children beside the root).
    (project_dir / SHEET_DIR).mkdir(exist_ok=True)
    keep = {(project_dir / f).resolve() for f in children}
    for old in list(project_dir.glob(f"{project_name}-*.kicad_sch")) + \
            list((project_dir / SHEET_DIR).glob(f"{project_name}-*.kicad_sch")):
        if old.resolve() not in keep:
            old.unlink()
    for fname, text in children.items():
        (project_dir / fname).write_text(text, encoding="utf-8")
    sch_path.write_text(root_text, encoding="utf-8")
    print(f"Wrote {sch_path} (root) + {len(children)} child sheets")
    print(f"  {n_placed} symbols placed across {len(children)} sheets, "
          f"{n_defs} unique definitions embedded.")
    # Canonicalize EACH child + the root. `sch upgrade` on the root does NOT reindent
    # child files, and our embedded lib defs carry the source lib's indentation — so
    # each child must be upgraded individually to normalize it to KiCad's format.
    for fname in children:
        canonicalize_sch(cli, project_dir / fname)
    canonicalize_sch(cli, sch_path)
    print("  validated + canonicalized every sheet via kicad-cli sch upgrade")

    # keep the project's page list in sync (root = page 1, children = pages 2..N),
    # so the GUI doesn't renumber on open. Targeted replace of just the "sheets" array.
    pro_txt = pro.read_text()
    items = [[root_uuid, "Root"]] + [[suid, name] for name, _fn, suid in sheets]
    arr = ",\n".join(f'    [\n      "{u}",\n      "{n}"\n    ]' for u, n in items)
    new_txt = re.sub(r'"sheets":\s*\[.*?\n  \]', f'"sheets": [\n{arr}\n  ]',
                     pro_txt, count=1, flags=re.S)
    if new_txt != pro_txt:
        pro.write_text(new_txt)
        print(f"  updated {pro.name} page list ({len(items)} pages)")

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
            print(f"    {r['refdes']} ({r['note']})  symbol={r['symbol']!r}")

    s1, s2 = write_lib_tables(project_dir, symbols.resolve())
    print(f"  sym-lib-table: {s1}, fp-lib-table: {s2}")


STUB = 2.54   # mm: length of the stub wire drawn from each pin out to its label
MAX_WIRE_FANOUT = 4


def read_net_kinds(nets_path):
    """Read the nets CSV 'kind' column -> {net: 'gnd'|'pwr'|'sig'}. The CSV is the SOURCE OF TRUTH for
    which nets are ground/power/signal — the tool never guesses from net names. 'kind' is per-net, so
    every row of a net must agree (validated); a blank/missing kind means signal."""
    kinds = {}
    for r in csvmod.DictReader(open(nets_path, newline="", encoding="utf-8")):
        net = (r.get("net") or "").strip()
        if not net or net.startswith("TODO"):
            continue
        k = (r.get("kind") or "").strip().lower() or "sig"
        if net in kinds and kinds[net] != k:
            sys.exit(f"ERROR: net '{net}' has inconsistent 'kind' in {Path(nets_path).name} "
                     f"({kinds[net]} vs {k}); every row of a net must agree.")
        kinds[net] = k
    return kinds


# --------------------------------------------------------------------------- #
# Pin geometry + instance parsing
# --------------------------------------------------------------------------- #
def parse_pins(symbol):
    """(number, name, lx, ly, angle) for each pin, from the symbol def block."""
    out, b = [], symbol.block
    for pm in re.finditer(r"\(pin\s", b):
        seg = b[pm.start():match_paren(b, pm.start())]
        at = re.search(r"\(at ([-\d.]+) ([-\d.]+) (\d+)\)", seg)
        num = re.search(r'\(number "([^"]*)"', seg)
        nm = re.search(r'\(name "([^"]*)"', seg)
        if at and num:
            out.append((num.group(1), nm.group(1) if nm else "",
                        float(at.group(1)), float(at.group(2)), int(at.group(3))))
    return out


def parse_instances(sch_text):
    """[(refdes, lib_id, px, py, rot)] for each placed symbol."""
    out = []
    for m in re.finditer(r"\n\t\(symbol\n", sch_text):
        blk = sch_text[m.start() + 1:match_paren(sch_text, m.start() + 1)]
        ref = re.search(r'\(reference "([^"]+)"', blk)
        lib = re.search(r'\(lib_id "([^"]+)"', blk)
        at = re.search(r"\(at ([-\d.]+) ([-\d.]+) (\d+)\)", blk)
        if ref and lib and at:
            out.append((ref.group(1), lib.group(1),
                        float(at.group(1)), float(at.group(2)), int(at.group(3))))
    return out


def pin_sheet_xy(px, py, rot, lx, ly):
    """Lib pin (lx,ly) -> sheet coords for a symbol at (px,py) rotation `rot`.

    Verified against KiCad ERC for rot 0: sheet = (px+lx, py-ly). Other
    rotations rotate the (lx,-ly) vector by `rot` degrees clockwise on screen.
    """
    dx, dy = lx, -ly                      # lib Y-up -> sheet Y-down
    if rot == 90:
        dx, dy = dy, -dx
    elif rot == 180:
        dx, dy = -dx, -dy
    elif rot == 270:
        dx, dy = -dy, dx
    return px + dx, py + dy


# --------------------------------------------------------------------------- #
# Stage 2: render nets CSV -> labels on the schematic
# --------------------------------------------------------------------------- #
# pin lib-angle -> (label angle, horizontal justify) so text reads OUTWARD, away
# from the body: left-side pins (ang 0) get right-justified text extending left, etc.
_LABELDIR = {0: (0, "right"), 180: (0, "left"), 90: (90, "right"), 270: (90, "left")}


def _label(net, x, y, ang, justify="left"):
    """LOCAL label — connects same-named labels WITHIN one sheet. Use for a net
    whose pins all live in one subcircuit (one child sheet)."""
    v = net.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(label "{v}"\n\t\t(at {x:.4f} {y:.4f} {ang})\n\t\t(effects\n'
            f"\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n"
            f"\t\t\t(justify {justify} bottom)\n\t\t)\n\t\t(uuid \"{_u()}\")\n\t)\n")


def _glabel(net, x, y, ang, justify="left"):
    """GLOBAL label — connects by name ACROSS child sheets. Use for a net whose
    pins span >1 subcircuit; a plain local label would not cross the sheet boundary."""
    v = net.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(global_label "{v}"\n\t\t(shape bidirectional)\n\t\t(at {x:.4f} {y:.4f} {ang})\n'
            f"\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n"
            f"\t\t\t(justify {justify})\n\t\t)\n\t\t(uuid \"{_u()}\")\n\t)\n")


def label_emitter(net, global_nets):
    """Pick the local- or global-label emitter for `net`."""
    return _glabel if net in global_nets else _label


def compute_global_nets(nets_path, subcircuit):
    """Nets whose pins span >1 subcircuit — these need GLOBAL labels to connect
    across child sheets. A net entirely within one subcircuit stays LOCAL (cleaner,
    and avoids a lone global label reading as an isolated pin in ERC)."""
    import csv as _csv
    seen = {}
    for r in _csv.DictReader(open(nets_path)):
        net = (r["net"] or "").strip()
        if not net or net.startswith("TODO"):
            continue
        seen.setdefault(net, set()).add(subcircuit.get(r["refdes"], ""))
    return {n for n, subs in seen.items() if len(subs) > 1}


def find_children(project_dir, name):
    """From the root schematic, return [(sheet_name, child_path, inst_path)] where
    inst_path = /<root_uuid>/<sheet_uuid> (the hierarchical path a symbol on that
    child must carry). Falls back to treating the root itself as the sole sheet if
    it holds no (sheet ...) blocks (i.e. still a flat schematic)."""
    root_path = project_dir / f"{name}.kicad_sch"
    txt = root_path.read_text(encoding="utf-8")
    root_uuid = re.search(r'\(uuid "([^"]+)"', txt).group(1)
    out = []
    for m in re.finditer(r"\n\t\(sheet\b", txt):
        blk = txt[m.start() + 1:match_paren(txt, m.start() + 1)]
        suid = re.search(r'\(uuid "([^"]+)"', blk)
        fn = re.search(r'"Sheetfile"\s+"([^"]+)"', blk)
        sn = re.search(r'"Sheetname"\s+"([^"]+)"', blk)
        if suid and fn:
            out.append((sn.group(1) if sn else fn.group(1),
                        project_dir / fn.group(1), f"/{root_uuid}/{suid.group(1)}"))
    if not out:                                   # flat schematic (no child sheets)
        out.append((name, root_path, f"/{root_uuid}"))
    return out


def _wire(x1, y1, x2, y2):
    return (f"\t(wire\n\t\t(pts\n\t\t\t(xy {x1:.4f} {y1:.4f}) (xy {x2:.4f} {y2:.4f})\n\t\t)\n"
            f"\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n"
            f"\t\t(uuid \"{_u()}\")\n\t)\n")


def _junction(x, y):
    return (f"\t(junction\n\t\t(at {x:.4f} {y:.4f})\n\t\t(diameter 0)\n"
            f"\t\t(color 0 0 0 0)\n\t\t(uuid \"{_u()}\")\n\t)\n")


def _no_connect(x, y):
    return (f"\t(no_connect\n\t\t(at {x:.4f} {y:.4f})\n\t\t(uuid \"{_u()}\")\n\t)\n")


# outward sheet direction -> instance rotation so the glyph points AWAY from the part
_GND_ROT = {(0, 1): 0, (0, -1): 180, (1, 0): 90, (-1, 0): 270}
_RAIL_ROT = {(0, -1): 0, (0, 1): 180, (1, 0): 270, (-1, 0): 90}


def load_power_defs(cli, nets, ground_nets, with_flag=False):
    """Embed-ready power-symbol defs for `nets`. Nets in `ground_nets` use stock power:GND;
    the rest clone stock power:+3V3 renamed to the net. With `with_flag`, also embed power:PWR_FLAG.
    Returns {lib_id: def}."""
    import shutil
    import tempfile
    src = Path(cli).parent.parent / "SharedSupport/symbols/power.kicad_sym"
    tmp = Path(tempfile.mkdtemp(prefix="pwr_"))
    dst = tmp / "power.kicad_sym"
    shutil.copy(src, dst)
    subprocess.run([cli, "sym", "upgrade", "--force", str(dst)],
                   capture_output=True, text=True)
    txt = dst.read_text(encoding="utf-8")

    def block(name):
        m = re.search(r'\n\t\(symbol "%s"' % re.escape(name), txt)
        return txt[m.start() + 1:match_paren(txt, m.start() + 1)]

    gnd, rail = block("GND"), block("+3V3")
    flag = block("PWR_FLAG") if with_flag else None
    shutil.rmtree(tmp, ignore_errors=True)
    out = {}
    if flag:
        out["power:PWR_FLAG"] = flag.replace('(symbol "PWR_FLAG"', '(symbol "power:PWR_FLAG"', 1)
    for net in set(nets):
        if net in ground_nets:
            out["power:GND"] = gnd.replace('(symbol "GND"', '(symbol "power:GND"', 1)
        else:
            d = rail.replace("+3V3", net)
            d = d.replace('(symbol "%s"' % net, '(symbol "power:%s"' % net, 1)
            out["power:%s" % net] = d
    return out


def _power_inst(net, x, y, rot, ref, inst_path, ground=False, outdir=(0, 1)):
    lib = "power:GND" if ground else "power:%s" % net
    val = "GND" if ground else net
    ta = horiz_text_ang(rot)                  # keep net-name text horizontal
    # Label sits BEYOND the arrow, in the pin's outward direction — never back on
    # the pin/wire. (A hardcoded y+2.54 put an upward rail's label onto its pin.)
    ox, oy = outdir
    lx, ly = x + ox * 3.81, y + oy * 3.81
    just = "right" if ox < 0 else "left" if ox > 0 else None   # KiCad has no 'center'
    props = (_prop("Reference", ref, x, y, hide=True)
             + _prop("Value", val, lx, ly, ang=ta, just=just))
    p = inst_path if inst_path.startswith("/") else "/" + inst_path   # accept bare uuid or full path
    return (f"\t(symbol\n\t\t(lib_id \"{lib}\")\n\t\t(at {x:.2f} {y:.2f} {rot})\n"
            f"\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n"
            f"\t\t(on_board yes)\n\t\t(dnp no)\n\t\t(uuid \"{_u()}\")\n{props}"
            f"\t\t(instances\n\t\t\t(project \"gb3\"\n\t\t\t\t(path \"{p}\"\n"
            f"\t\t\t\t\t(reference \"{ref}\")\n\t\t\t\t\t(unit 1)\n"
            f"\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n")


def _flag_inst(x, y, ref, inst_path):
    """A PWR_FLAG symbol at (x,y). Placed coincident with a power:<net> symbol, its pin marks that net
    as DRIVEN for ERC (a PWR_FLAG pin is a power-output), silencing 'power input not driven'."""
    p = inst_path if inst_path.startswith("/") else "/" + inst_path
    props = (_prop("Reference", ref, x, y, hide=True)
             + _prop("Value", "PWR_FLAG", x, y - 2.54, hide=True))
    return (f"\t(symbol\n\t\t(lib_id \"power:PWR_FLAG\")\n\t\t(at {x:.2f} {y:.2f} 0)\n"
            f"\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n"
            f"\t\t(on_board yes)\n\t\t(dnp no)\n\t\t(uuid \"{_u()}\")\n{props}"
            f"\t\t(instances\n\t\t\t(project \"gb3\"\n\t\t\t\t(path \"{p}\"\n"
            f"\t\t\t\t\t(reference \"{ref}\")\n\t\t\t\t\t(unit 1)\n"
            f"\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n")


def _seg_int(s1, s2, eps=0.05):
    """True if two axis-aligned segments touch/cross/overlap (any shared point)."""
    ax1, ay1, ax2, ay2 = s1
    bx1, by1, bx2, by2 = s2
    if (max(ax1, ax2) < min(bx1, bx2) - eps or min(ax1, ax2) > max(bx1, bx2) + eps or
            max(ay1, ay2) < min(by1, by2) - eps or min(ay1, ay2) > max(by1, by2) + eps):
        return False                              # bounding boxes disjoint
    a_h, b_h = abs(ay1 - ay2) < eps, abs(by1 - by2) < eps
    if a_h and b_h:
        return abs(ay1 - by1) < eps               # both horizontal: same line?
    if not a_h and not b_h:
        return abs(ax1 - bx1) < eps               # both vertical: same line?
    return True                                   # perpendicular + bbox overlap => cross


def _pt_on(p, s, eps=0.05):
    px, py = p
    x1, y1, x2, y2 = s
    if abs(y1 - y2) < eps:
        return abs(py - y1) < eps and min(x1, x2) - eps <= px <= max(x1, x2) + eps
    return abs(px - x1) < eps and min(y1, y2) - eps <= py <= max(y1, y2) + eps


def _strip(sch_text, token):
    """Remove all top-level (token ...) blocks (idempotent re-render).

    The class char [\\s(] after the token matches whether the element opens with
    a space (`(label "..."`) or a newline (`(wire\\n`). A trailing-space-only
    pattern silently never matched `(wire`, so wires accumulated on re-render.
    """
    result = sch_text
    while True:
        m = re.search(r"\n\t\(%s[\s(]" % re.escape(token), result)
        if not m:
            break
        end = match_paren(result, m.start() + 1)
        result = result[:m.start()] + result[end:]
    return result


def _strip_power(sch_text):
    """Remove previously-emitted power-symbol instances (#PWR*) and their
    power:* defs from lib_symbols, so re-render is idempotent."""
    t = sch_text
    while True:
        hit = None
        for m in re.finditer(r"\n\t\(symbol\n", t):
            blk = t[m.start() + 1:match_paren(t, m.start() + 1)]
            if re.search(r'\(reference "#PWR', blk):
                hit = (m.start(), match_paren(t, m.start() + 1)); break
        if not hit:
            break
        t = t[:hit[0]] + t[hit[1]:]
    while True:
        m = re.search(r'\n\t\t\(symbol "power:', t)
        if not m:
            break
        t = t[:m.start()] + t[match_paren(t, m.start() + 1):]
    return t


def render(sch_text, csv_path, by_libid, subcircuit, cli, direct_wire=False,
           inst_path=None, global_nets=frozenset(), net_kind=None, flag_nets=frozenset()):
    """Return (new_sch_text, counts) from the nets CSV, for ONE sheet.

    Only pins whose symbols are placed on THIS sheet get emitted (the pinxy guards
    filter the CSV automatically), so calling this once per child sheet wires the
    whole hierarchy. `inst_path` (/<root>/<sheet>) is the hierarchical path power
    symbols on this sheet must carry.

    Ground/rail nets get power *symbols* (power:GND / power:<rail>); bus & signal
    labels get a short stub off the pin; unconnected pins get a no-connect X;
    non-global signal clusters are direct-wired when --direct-wire.

    `net_kind` (from the nets CSV 'kind' column) decides ground/power/signal — the tool never guesses
    from net names.
    """
    kinds = net_kind or {}
    def _gnd(n):
        return kinds.get(n) == "gnd"
    def _pwr(n):
        return kinds.get(n) in ("gnd", "pwr")
    net_pins, unconnected = {}, []
    with open(csv_path, newline="", encoding="utf-8") as f:
        for r in csvmod.DictReader(f):
            net = r["net"].strip()
            if net and not net.startswith("TODO"):
                net_pins.setdefault(net, []).append((r["refdes"], r["pin"]))
            else:
                unconnected.append((r["refdes"], r["pin"]))

    # strip global_label too — its token isn't a prefix-match of "label", so a
    # re-render would otherwise leave stale global labels that collide with new ones.
    sch_text = _strip_power(_strip(_strip(_strip(_strip(_strip(
        sch_text, "global_label"), "label"), "wire"), "junction"), "no_connect"))
    insts = parse_instances(sch_text)
    if not insts:
        sys.exit("ERROR: parsed 0 symbol instances from the schematic — "
                 "read-back regex mismatch; aborting rather than emit nothing.")
    if re.search(r"\(mirror [xy]\)", sch_text):
        print("  WARNING: schematic has mirrored symbol(s); mirror is NOT applied "
              "to pin geometry — labels on mirrored parts may miss their pins.")
    root = re.search(r'\(uuid "([^"]+)"', sch_text).group(1)
    pwr_path = inst_path or f"/{root}"        # hierarchical path for power symbols on this sheet

    # (refdes, pin) -> sheet geometry
    pinxy = {}
    for refdes, lib_id, px, py, rot in insts:
        sym = by_libid.get(lib_id)
        if not sym:
            continue
        for num, name, lx, ly, ang in parse_pins(sym):
            pinxy[(refdes, num)] = (px, py, rot, lx, ly, ang)

    pincnt = {}                       # pins per component (2-pin => a passive)
    for (ref, _p) in pinxy:
        pincnt[ref] = pincnt.get(ref, 0) + 1

    # every pin's sheet coordinate — wires must not cross any of these
    allpins = set()
    for (ref, pn), g in pinxy.items():
        x, y = pin_sheet_xy(g[0], g[1], g[2], g[3], g[4])
        allpins.add((round(x, 2), round(y, 2)))

    def outward(ang):
        a = math.radians(ang)
        return round(-math.cos(a)), round(math.sin(a))

    def route_net(cpins):
        """Route a cluster: each pin exits in its OWN facing direction to a stub,
        then up to a shared horizontal trunk in the channel above. Works for IC
        side-pins (which a straight-up comb would run through their neighbours).

        Returns (segments, junctions, label_xy), or None if any segment would
        cross a pin that isn't one of this cluster's own tips.
        """
        ent = []
        for ref, pin in cpins:
            g = pinxy.get((ref, pin))
            if not g:
                return None
            px, py, rot, lx, ly, ang = g
            tx, ty = pin_sheet_xy(px, py, rot, lx, ly)
            a = math.radians(ang)
            ox, oy = round(-math.cos(a)), round(math.sin(a))   # sheet outward (rot 0)
            ent.append((round(tx, 2), round(ty, 2), ox, oy))
        tips = {(tx, ty) for tx, ty, _, _ in ent}
        segs, axes = [], []
        for tx, ty, ox, oy in ent:
            ax, ay = round(tx + STUB * ox, 2), round(ty + STUB * oy, 2)
            segs.append((tx, ty, ax, ay))         # outward stub
            axes.append((ax, ay))
        trunkY = round(min(ay for _, ay in axes) - 5.08, 2)
        for ax, ay in axes:
            segs.append((ax, ay, ax, trunkY))     # up to the trunk
        xmin = min(ax for ax, _ in axes)
        xmax = max(ax for ax, _ in axes)
        segs.append((xmin, trunkY, xmax, trunkY))  # the trunk
        for seg in segs:                           # no pin (except our own tips) on any seg
            for p in allpins:
                if p not in tips and _pt_on(p, seg):
                    return None
        juncs = [(ax, trunkY) for ax, _ in axes if xmin < ax < xmax]
        return segs, juncs, (xmin, trunkY)

    elems, cnt = [], {"labels": 0, "wires": 0, "power": 0, "nc": 0}
    drawn_segs, label_pts = [], []    # already-emitted wires / channel labels
    pwr_n = [0]
    power_nets_used = set()

    def tip(ref, pin):
        g = pinxy.get((ref, pin))
        return pin_sheet_xy(g[0], g[1], g[2], g[3], g[4]) if g else None

    def stub_out(ref, pin):
        """(tip, stub_end, outward) with a clear stub, or (tip, tip, out) if blocked."""
        g = pinxy[(ref, pin)]
        px, py, rot, lx, ly, ang = g
        tx, ty = pin_sheet_xy(px, py, rot, lx, ly)
        tx, ty = round(tx, 2), round(ty, 2)
        ox, oy = outward(ang)
        sx, sy = round(tx + STUB * ox, 2), round(ty + STUB * oy, 2)
        clear = ((sx, sy) not in allpins and
                 not any(p != (tx, ty) and _pt_on(p, (tx, ty, sx, sy)) for p in allpins))
        return (tx, ty), ((sx, sy) if clear else (tx, ty)), (ox, oy)

    def place_label(ref, pin, net):       # signal/bus label on a short stub
        if (ref, pin) not in pinxy:
            return
        (tx, ty), (ax, ay), _ = stub_out(ref, pin)
        if (ax, ay) != (tx, ty):
            elems.append(_wire(tx, ty, ax, ay)); cnt["wires"] += 1
        ang = pinxy[(ref, pin)][5]
        lab_ang, just = _LABELDIR.get(ang % 360, (0, "left"))
        elems.append(label_emitter(net, global_nets)(net, ax, ay, lab_ang, just))
        cnt["labels"] += 1

    def place_power(ref, pin, net):       # ground/rail power symbol on a short stub
        if (ref, pin) not in pinxy:
            return
        (tx, ty), (ax, ay), (ox, oy) = stub_out(ref, pin)
        if (ax, ay) != (tx, ty):
            elems.append(_wire(tx, ty, ax, ay)); cnt["wires"] += 1
        rot = (_GND_ROT if _gnd(net) else _RAIL_ROT).get((ox, oy), 0)
        pwr_n[0] += 1
        ref_pwr = "#PWR%04d" % pwr_n[0]
        elems.append(_power_inst(net, ax, ay, rot, ref_pwr, pwr_path,
                                 ground=_gnd(net), outdir=(ox, oy)))
        power_nets_used.add(net); cnt["power"] += 1

    def place_nc(ref, pin):               # intentionally-unconnected pin
        g = pinxy.get((ref, pin))
        if not g:
            return
        tx, ty = pin_sheet_xy(g[0], g[1], g[2], g[3], g[4])
        elems.append(_no_connect(tx, ty)); cnt["nc"] += 1

    def gkey(ref):
        sc = subcircuit.get(ref, "")
        return sc if sc else "\x00" + ref     # ungrouped -> unique singleton

    for net, pins in net_pins.items():
        if _pwr(net):
            for ref, pin in pins:
                place_power(ref, pin, net)
            continue
        glob = False          # direct-wire disabled; label-vs-global comes from global_nets (span-based)
        groups = {}
        for ref, pin in pins:
            groups.setdefault(gkey(ref), []).append((ref, pin))
        spans = len(groups) > 1          # net touches >1 subcircuit
        for gk, gpins in groups.items():
            explicit = not gk.startswith("\x00")     # part had a real subcircuit
            base_ok = (direct_wire and explicit and not glob
                       and len({r for r, _ in gpins}) >= 2)
            # Try to wire the whole cluster; if that won't route, try passives
            # only (label the ICs); else label everything.
            passives = [(r, p) for (r, p) in gpins if pincnt.get(r, 99) <= 2]
            candidates = []
            if base_ok and 2 <= len(gpins) <= MAX_WIRE_FANOUT:
                candidates.append(gpins)
            if base_ok and len({r for r, _ in passives}) >= 2 \
                    and 2 <= len(passives) <= MAX_WIRE_FANOUT:
                candidates.append(passives)

            routed, wired = None, set()
            for cand in candidates:
                rn = route_net(cand)
                if not rn:
                    continue
                segs, juncs, (lx0, ly0) = rn
                clash = (any(_seg_int(s, d) for s in segs for d in drawn_segs)
                         or any(_pt_on((lx0, ly0), d) for d in drawn_segs)
                         or any(_pt_on(lp, s) for s in segs for lp in label_pts))
                if not clash:
                    routed, wired = rn, set(cand)
                    break

            if routed:
                segs, juncs, (lx0, ly0) = routed
                for (x1, y1, x2, y2) in segs:
                    elems.append(_wire(x1, y1, x2, y2)); cnt["wires"] += 1
                for (jx, jy) in juncs:
                    elems.append(_junction(jx, jy))
                drawn_segs.extend(segs)
                # a fully-wired net inside ONE subcircuit needs no label; only
                # label to unify across subcircuits or with un-wired pins here.
                if spans or any((r, p) not in wired for r, p in gpins):
                    elems.append(label_emitter(net, global_nets)(net, lx0, ly0, 0))
                    cnt["labels"] += 1
                    label_pts.append((lx0, ly0))
                for ref, pin in gpins:      # pins left out of the route -> labels
                    if (ref, pin) not in wired:
                        place_label(ref, pin, net)
            else:
                for ref, pin in gpins:      # unwired (or unroutable): label every pin
                    place_label(ref, pin, net)

    for ref, pin in unconnected:            # intentionally-unconnected pins
        place_nc(ref, pin)

    # PWR_FLAG drivers: one per declared power/ground net, coincident with a power:<net> symbol.
    # Power symbols are global, so a single flag here marks that net driven board-wide (clears ERC's
    # 'power input not driven'). Placed in a reserved row above the circuit.
    fx, fy = 12.7, 6.35
    for net in sorted(flag_nets):
        pwr_n[0] += 1
        # PWR_FLAG pin at (fx,fy) -> a short wire -> a GLOBAL label of the net name. The label ties the
        # flag to the power net BY NAME (a power net's name == its global-label text), so no pin-offset
        # geometry is needed; the flag's power-output pin then marks that net driven for ERC.
        elems.append(_flag_inst(fx, fy, "#FLG%04d" % pwr_n[0], pwr_path))
        elems.append(_wire(fx, fy, fx + 5.08, fy))
        elems.append(_glabel(net, fx + 5.08, fy, 0))
        fx += 20.32

    # embed the power-symbol defs we used into lib_symbols (+ PWR_FLAG when flags placed)
    if power_nets_used or flag_nets:
        defs = load_power_defs(cli, power_nets_used, {n for n in power_nets_used if _gnd(n)},
                               with_flag=bool(flag_nets))
        lm = re.search(r"\n\t\(lib_symbols\n", sch_text)
        close = match_paren(sch_text, lm.start() + 1)
        blob = "".join("\t\t%s\n" % d for d in defs.values())
        sch_text = sch_text[:close - 1] + blob + sch_text[close - 1:]

    # insert new elements before the trailing metadata. Anchor to a TOP-LEVEL
    # block (`\n\t(` = one-tab indent) — a plain str.find("\t(embedded_fonts")
    # also matches the two-tab "\t\t(embedded_fonts no)" NESTED inside a lib-symbol
    # def, which would splice the elements into lib_symbols and corrupt the sheet.
    m = (re.search(r"\n\t\(sheet_instances", sch_text)
         or re.search(r"\n\t\(embedded_fonts", sch_text))
    ins = m.start() + 1 if m else sch_text.rstrip().rfind(")")
    new = sch_text[:ins] + "".join(elems) + sch_text[ins:]
    return new, cnt


# --------------------------------------------------------------------------- #
# Verification: exported netlist vs the nets CSV
# --------------------------------------------------------------------------- #
def netlist_check(cli, sch_path, nets_path):
    """Confirm every rendered (refdes,pin,net) actually lands on that net.

    ERC can't catch a label placed a fraction off a pin, but the exported
    netlist shows true node membership. Returns [(ref,pin,want,got), ...].
    """
    tmp = sch_path.parent / (sch_path.stem + "-netcheck.net")
    r = subprocess.run([cli, "sch", "export", "netlist", "--format",
                        "kicadsexpr", "-o", str(tmp), str(sch_path)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ERROR: netlist export failed:\n" + (r.stderr or r.stdout))
    txt = tmp.read_text(encoding="utf-8")
    tmp.unlink()
    # KiCad names LOCAL-label nets with a sheet-path prefix (e.g. "soc/HOSC_OUT") and root nets with a
    # leading "/". Compare on the bare net name (final path segment) so a correctly-connected local net
    # is NOT reported as a mismatch.
    def basenet(n):
        return n.rsplit("/", 1)[-1] if n else n
    actual = {}
    sec = txt[txt.index("(nets"):]
    for blk in re.split(r"\n\t\t\(net\b", sec):
        nm = re.search(r'\(name "([^"]*)"', blk)
        if not nm:
            continue
        net = basenet(nm.group(1))
        for nd in re.finditer(r'\(node\s+\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk):
            actual[(nd.group(1), nd.group(2))] = net
    miss = []
    with open(nets_path, newline="", encoding="utf-8") as f:
        for row in csvmod.DictReader(f):
            net = row["net"].strip()
            got = actual.get((row["refdes"], row["pin"]))
            if net and not net.startswith("TODO"):
                if got != basenet(net):                    # landed on wrong/no net
                    miss.append((row["refdes"], row["pin"], net, got))
            elif got and not got.startswith("unconnected"):  # stray connection
                miss.append((row["refdes"], row["pin"], "(unconnected)", got))
    return miss


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def _wire_stage(project, symbols, nets, only):
    """WIRE stage: render net-label connectivity from the nets CSV onto the already-placed sheets
    (label-only). `only` (list|None) relabels just those subcircuit sheets, leaving the rest untouched;
    global-vs-local is still computed across the whole board."""
    cli = find_kicad_cli()
    project_dir = project.resolve()
    pro = next(project_dir.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project_dir}")
    name = pro.stem
    sch_path = project_dir / f"{name}.kicad_sch"
    nets_path = nets or (project_dir / f"{name}-nets.csv")
    if not sch_path.exists():
        sys.exit(f"ERROR: {sch_path} not found (run the place stage first)")

    print(f"Loading symbol libraries from {symbols}...")
    _by_lcsc, by_libid = load_symbols(symbols.resolve(), cli)
    children = find_children(project_dir, name)   # [(sheet_name, path, inst_path)]

    if only:
        child_names = {n for n, _p, _i in children}
        unknown = [g for g in only if g not in child_names]
        if unknown:
            sys.exit(f"ERROR: --only names are not sheets: {unknown}\n"
                     f"  available: {', '.join(sorted(child_names))}")

    # subcircuit membership (refdes -> group) comes from the BOM CSV. A blank
    # subcircuit is its OWN singleton group (keyed by refdes) — NOT the section:
    # direct-wiring is opt-in and only safe inside a tight, explicitly-grouped
    # box (§2.4), so ungrouped parts stay label-only.
    bom_path = project_dir / f"{name}-bom.csv"
    subcircuit = {}
    if bom_path.exists():
        for r in read_csv(bom_path):
            subcircuit[r["refdes"]] = r["subcircuit"]   # "" = ungrouped (singleton)

    print(f"Rendering GLOBAL labels from {nets_path.name} onto {len(children)} sheet(s)...")
    print("  NOTE: render replaces ALL labels/wires on each sheet — hand-drawn "
          "connections are not preserved (use the relayout scripts for those).")
    global_nets = compute_global_nets(nets_path, subcircuit)   # span >1 subcircuit -> global label
    net_kind = read_net_kinds(nets_path)                        # CSV declares gnd/pwr/sig
    flag_all = {n for n, k in net_kind.items() if k in ("gnd", "pwr")}   # each gets one PWR_FLAG driver
    flag_host = children[0][0] if children else None            # all flags on the first child sheet
    print(f"  {len(global_nets)} nets span >1 sheet (global labels); the rest stay local")
    print(f"  {len(flag_all)} power/ground net(s) get a PWR_FLAG (on sheet '{flag_host}')")
    tot = {"labels": 0, "wires": 0, "power": 0, "nc": 0}
    rendered = []
    for sname, cpath, ipath in children:
        if only and sname not in only:
            continue                            # leave this sheet's labels/wires untouched
        ctext = cpath.read_text(encoding="utf-8")
        new_text, cnt = render(ctext, nets_path, by_libid, subcircuit, cli,
                               direct_wire=False, inst_path=ipath, global_nets=global_nets,
                               net_kind=net_kind, flag_nets=(flag_all if sname == flag_host else frozenset()))
        cpath.write_text(new_text, encoding="utf-8")
        rendered.append(cpath)
        for k in tot:
            tot[k] += cnt[k]
    for cpath in rendered:                      # normalize + validate each relabeled child, then root
        if cpath != sch_path:
            canonicalize_sch(cli, cpath)
    canonicalize_sch(cli, sch_path)
    print(f"  {tot['labels']} labels + {tot['wires']} wires + {tot['power']} power "
          f"symbols + {tot['nc']} no-connects across {len(rendered)} sheet(s). "
          f"Validated {sch_path.name}.")
    # (ERC + netlist check run once, as the front-door validate_full gate.)


# --------------------------------------------------------------------------- #
# Front door + gating validation
# --------------------------------------------------------------------------- #
def validate_full(cli, nets_path, sch_path):
    """GATING validation of the built schematic. Returns True iff ALL hold:
      1. every pin has a net — no TODO_* rows in the nets CSV (blank net = an intentional no-connect, OK);
      2. ERC passes — zero error-severity violations and zero unconnected-pin violations;
      3. every rendered net actually landed on its pin (netlist_check).
    Warnings are reported but tolerated. Prints a per-check OK/FAIL summary."""
    print("\n=== VALIDATE ===")
    ok = True

    # 1. every pin assigned (TODO_* = unfilled work). Blank net = intentional NC (render draws no_connect).
    todo = []
    with open(nets_path, newline="", encoding="utf-8") as f:
        for row in csvmod.DictReader(f):
            if (row.get("net") or "").strip().startswith("TODO"):
                todo.append(f"{row['refdes']}.{row['pin']}")
    if todo:
        ok = False
        print(f"  FAIL: {len(todo)} pin(s) have no net (TODO_* in {nets_path.name}): "
              + ", ".join(todo[:12]) + (" …" if len(todo) > 12 else ""))
    else:
        print(f"  OK:   every pin in {nets_path.name} has a net (or an intentional no-connect)")

    # 2. ERC (json) — gate on error-severity + unconnected-pin; report warnings
    rpt = sch_path.with_name(sch_path.stem + "-erc.json")
    subprocess.run([cli, "sch", "erc", "--format", "json", "-o", str(rpt), str(sch_path)],
                   capture_output=True, text=True)
    errs = warns = unconn = 0
    try:
        data = json.loads(rpt.read_text(encoding="utf-8"))
        for sheet in data.get("sheets", []):
            for v in sheet.get("violations", []):
                blob = (str(v.get("type", "")) + " " + str(v.get("description", ""))).lower()
                if "not connected" in blob or "unconnected" in blob:
                    unconn += 1
                sev = str(v.get("severity", "")).lower()
                errs += sev == "error"
                warns += sev == "warning"
    except Exception as e:
        ok = False
        print(f"  FAIL: could not parse ERC report ({e})")
    if errs or unconn:
        ok = False
        print(f"  FAIL: ERC — {errs} error(s), {unconn} unconnected-pin violation(s) (report: {rpt.name})")
    else:
        print(f"  OK:   ERC passes (0 errors, 0 unconnected pins; {warns} warning(s))")

    # 3. every rendered net landed on its intended pin
    miss = netlist_check(cli, sch_path, nets_path)
    if miss:
        ok = False
        print(f"  FAIL: {len(miss)} net(s) did not land on their pin:")
        for ref, pin, want, got in miss[:12]:
            print(f"        {ref}.{pin}: want {want}, got {got or 'unconnected'}")
    else:
        print("  OK:   every rendered net landed on its pin")

    print(f"  => {'PASS' if ok else 'FAIL'}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bom", type=Path, required=True, help="parts CSV (the place stage input)")
    ap.add_argument("--nets", type=Path, default=None,
                    help="nets CSV (the wire stage input; default <project>/<name>-nets.csv)")
    ap.add_argument("--project", type=Path, default=DEFAULT_PROJECT,
                    help="dir containing <name>.kicad_pro")
    ap.add_argument("--symbols", type=Path, default=DEFAULT_SYMBOLS)
    ap.add_argument("--only", nargs="+", metavar="SUBCIRCUIT", default=None,
                    help="regenerate ONLY these subcircuit child sheets (place + re-label), leaving every "
                         "other sheet untouched.")
    args = ap.parse_args()

    if not args.bom.exists():
        sys.exit(f"ERROR: BOM CSV not found: {args.bom}")
    project = args.project.resolve()

    print("=== PLACE (parts) ===")
    _place_stage(project, args.symbols, args.bom, args.only)

    print("\n=== WIRE (net labels) ===")
    _wire_stage(project, args.symbols, args.nets, args.only)

    # --- gate: every pin on a net + ERC clean + netlist landed (always runs) ---
    pro = next(project.glob("*.kicad_pro"), None)
    name = pro.stem if pro else "gb3"
    sch_path = project / f"{name}.kicad_sch"
    nets_path = args.nets or (project / f"{name}-nets.csv")
    if not validate_full(find_kicad_cli(), nets_path, sch_path):
        sys.exit("\nVALIDATION FAILED — the schematic is not clean (see findings above).")


if __name__ == "__main__":
    main()
