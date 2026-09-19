#!/usr/bin/env python3
"""
generate_schematic.py — build a hierarchical KiCad schematic from two source-of-truth CSVs: --bom
(parts + their subcircuit) and --nets (per-pin connectivity + kind). One child sheet per subcircuit +
a root, then a mandatory validation gate. The tool never invents connectivity or writes the CSVs back.
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
from collections import defaultdict
from pathlib import Path


# --------------------------------------------------------------------------- #
# Defaults + layout constants (relative to this file's location: <repo>/cad/)
# --------------------------------------------------------------------------- #
HERE = Path(__file__).resolve().parent
REPO = HERE.parent
DEFAULT_PROJECT = REPO / "projects/gameboy-v3/pcb/gb3"
DEFAULT_SYMBOLS = REPO / "cad/symbols"
LIBS = ["easyeda2kicad", "manual"]

MARGIN = 25.4          # sheet border (mm)
HEADER_GAP = 12.7      # gap below the sheet title
GAP = 10.16            # inter-part clearance added around each part's (label-inclusive) bbox
SHEET_W = 1100.0       # wrap parts to a new shelf past this width
SHEET_DIR = "sheets"   # child .kicad_sch files live in <project>/sheets/ to keep the root dir tidy
STUB = 2.54            # length of the stub wire drawn from each pin out to its label
LBL_ADV = 0.9          # estimated label-text advance per char at 1.27 size (over-estimate on purpose)
LBL_PAD = 1.27         # extra pad added to each label's reach


# --------------------------------------------------------------------------- #
# S-expression helpers + kicad-cli
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
    """text[open_idx] is '('; return index just past the matching ')', or -1. String-aware."""
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
    """Validate + reformat in place via KiCad's own parser (`sch upgrade --force`); fails if it rejects."""
    r = subprocess.run([cli, "sch", "upgrade", "--force", str(sch_path)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ERROR: KiCad rejected the generated schematic "
                 f"(sch upgrade failed):\n{r.stderr or r.stdout}")


# --------------------------------------------------------------------------- #
# Symbol library parsing
# --------------------------------------------------------------------------- #
class Symbol:
    __slots__ = ("nick", "name", "block", "ref", "ref_found", "footprint",
                 "value", "datasheet", "description",
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
        self._bbox()

    def _bbox(self):
        """Lib-space extent: pin tips + all body graphics (polylines/arcs/circles, not just rects).
        d* = drawn graphics only (pins excluded), for centering ref/value on asymmetric bodies."""
        xs, ys, dxs, dys = [], [], [], []
        for m in re.finditer(r"\(pin\s+\w+\s+\w+\s*\(at (-?[\d.]+) (-?[\d.]+)", self.block):
            xs.append(float(m.group(1))); ys.append(float(m.group(2)))
        for m in re.finditer(r"\((?:start|end|mid|xy) (-?[\d.]+) (-?[\d.]+)\)", self.block):
            xs.append(float(m.group(1))); ys.append(float(m.group(2)))
            dxs.append(float(m.group(1))); dys.append(float(m.group(2)))
        for m in re.finditer(r"\(center (-?[\d.]+) (-?[\d.]+)\)\s*\(radius ([\d.]+)\)", self.block):
            cx, cy, r = float(m.group(1)), float(m.group(2)), float(m.group(3))
            xs += [cx - r, cx + r]; ys += [cy - r, cy + r]
            dxs += [cx - r, cx + r]; dys += [cy - r, cy + r]
        if not xs:                            # degenerate: assume a small part
            xs, ys = [-2.54, 2.54], [-2.54, 2.54]
        self.lminx, self.lmaxx = min(xs), max(xs)
        self.lminy, self.lmaxy = min(ys), max(ys)
        self.dlminy, self.dlmaxy = (min(dys), max(dys)) if dys else (self.lminy, self.lmaxy)
        self.dlminx, self.dlmaxx = (min(dxs), max(dxs)) if dxs else (self.lminx, self.lmaxx)

    def _prop(self, key):
        m = re.search(r'\(property\s+"%s"\s+"((?:[^"\\]|\\.)*)"' % re.escape(key), self.block)
        return m.group(1).replace('\\"', '"').replace("\\\\", "\\") if m else None

    @property
    def lib_id(self):
        return f"{self.nick}:{self.name}"

    def embedded_def(self):
        return self.block.replace(f'(symbol "{self.name}"',
                                  f'(symbol "{self.lib_id}"', 1)


def load_symbols(symbols_dir, cli):
    """Upgrade throwaway copies of the libs to current format, parse. Returns {"nick:name" -> Symbol}."""
    tmp = Path(tempfile.mkdtemp(prefix="bom2sch_"))
    by_libid = {}
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
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not by_libid:
        sys.exit("ERROR: parsed 0 symbols from the libraries — the .kicad_sym read-back likely broke.")
    return by_libid


# --------------------------------------------------------------------------- #
# Parts CSV (strict): required columns refdes, subcircuit, symbol
# --------------------------------------------------------------------------- #
def read_csv(path):
    """Parse the parts CSV (required: refdes, subcircuit, symbol; extras ignored). Every row is a part;
    refdes and subcircuit must be non-blank and refdes unique — all hard errors, no fallbacks."""
    resolved = []
    with open(path, newline="", encoding="utf-8") as f:
        rd = csvmod.DictReader(f)
        missing = {"refdes", "subcircuit", "symbol"} - set(rd.fieldnames or [])
        if missing:
            sys.exit(f"ERROR: {Path(path).name} missing required column(s): "
                     f"{', '.join(sorted(missing))} (header: {rd.fieldnames})")
        for r in rd:
            resolved.append(dict(
                refdes=(r.get("refdes") or "").strip(),
                subcircuit=(r.get("subcircuit") or "").strip(),
                lcsc=(r.get("lcsc") or ""),
                symbol=(r.get("symbol") or "").strip(),
                note=(r.get("note") or ""),
            ))
    for col in ("refdes", "subcircuit"):
        bad = [r for r in resolved if not r[col]]
        if bad:
            detail = "; ".join((r["refdes"] or r["symbol"] or "?") for r in bad[:8])
            sys.exit(f"ERROR: {Path(path).name}: {len(bad)} row(s) have a BLANK {col}. First: {detail}")
    seen, dups = set(), set()
    for r in resolved:
        (dups if r["refdes"] in seen else seen).add(r["refdes"])
    if dups:
        sys.exit(f"ERROR: {Path(path).name}: duplicate refdes {sorted(dups)[:8]} (each must be unique).")
    return resolved


# --------------------------------------------------------------------------- #
# UUIDs we emit are DETERMINISTIC (uuid5 of stable content): sheet/file/root keep stable hierarchical
# instance paths (netlist provenance; lets --only match the existing root), and every element (symbol,
# label, wire, power, no-connect, title) is keyed on its content, so OUR lines don't churn across a
# rebuild. (KiCad still injects its own random per-pin instance UUIDs on `sch upgrade`, so the on-disk
# file is not fully byte-stable — but the electrical netlist is identical run to run.)
# --------------------------------------------------------------------------- #
_UUID_NS = uuidlib.UUID("6ba7b811-9dad-11d1-80b4-00c04fd430c8")


def _det_uuid(*parts):
    return str(uuidlib.uuid5(_UUID_NS, ":".join(parts)))


def sheet_slug(grp):
    """Filesystem-safe child-sheet slug for a subcircuit name."""
    return re.sub(r"[^A-Za-z0-9]+", "-", grp).strip("-").lower() or "misc"


# --------------------------------------------------------------------------- #
# s-expr emitters (byte formats KiCad canonicalizes on `sch upgrade`)
# --------------------------------------------------------------------------- #
def _prop(name, value, x, y, hide=False, just=None, ang=0):
    v = value.replace("\\", "\\\\").replace('"', '\\"')
    hide_line = "\t\t\t(hide yes)\n" if hide else ""
    just_line = f"\t\t\t\t(justify {just})\n" if just else ""
    return (f'\t\t(property "{name}" "{v}"\n\t\t\t(at {x:.2f} {y:.2f} {ang})\n'
            f"{hide_line}\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1.27 1.27)"
            f"\n\t\t\t\t)\n{just_line}\t\t\t)\n\t\t)\n")


def _symbol_block(lib_id, x, y, rot, props, ref, inst_path, project_name, in_bom, uid):
    """One placed (symbol ...): shared by parts (in_bom yes) and power/PWR_FLAG symbols (in_bom no)."""
    p = inst_path if inst_path.startswith("/") else "/" + inst_path
    return (f"\t(symbol\n\t\t(lib_id \"{lib_id}\")\n\t\t(at {x:.2f} {y:.2f} {rot})\n"
            f"\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom {in_bom})\n"
            f"\t\t(on_board yes)\n\t\t(dnp no)\n\t\t(uuid \"{uid}\")\n{props}"
            f"\t\t(instances\n\t\t\t(project \"{project_name}\"\n\t\t\t\t(path \"{p}\"\n"
            f"\t\t\t\t\t(reference \"{ref}\")\n\t\t\t\t\t(unit 1)\n"
            f"\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)\n")


def horiz_text_ang(rot):
    """Symbol-local field angle that renders text HORIZONTAL for a symbol at `rot`. KiCad rotates field
    text with a 90/270 symbol rotation (compensate 360-rot) but not for 180 — so 0 and 180 both want 0."""
    return (360 - rot) % 360 if rot in (90, 270) else 0


def ref_value_props(refdes, value, x, y, rot, side="left", bh=0.0, off=None, body_y=None, body_x=None):
    """Reference above / Value below (rot 0/180) or stacked to one side (90/270), always horizontal.
    body_y/body_x clear + centre on the DRAWN box so asymmetric bodies don't float the label off."""
    if rot in (90, 270):
        off = off if off is not None else max(bh + 1.27, 3.81)
        sx = x + (off if side == "right" else -off)
        just = "left" if side == "right" else "right"
        ta = horiz_text_ang(rot)
        return (_prop("Reference", refdes, sx, y - 1.27, just=just, ang=ta)
                + _prop("Value", value, sx, y + 1.27, just=just, ang=ta))
    if body_y is not None:
        lo, hi = body_y
        up, dn = (hi, -lo) if rot == 0 else (-lo, hi)
        above, below = max(up + 2.54, 2.54), max(dn + 2.54, 2.54)
    else:
        above = below = max(bh + 2.54, 2.54)
    cx = x
    if body_x is not None:
        c = (body_x[0] + body_x[1]) / 2.0
        cx = x + (c if rot == 0 else -c)
    return (_prop("Reference", refdes, cx, y - above)
            + _prop("Value", value, cx, y + below))


# value+unit anywhere in a description ("100nF", "2.2uH", "75KΩ", "10mΩ"); lookahead rejects MPN "25F"
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
    unit = {"uF": "µF", "uH": "µH", "KΩ": "k", "kΩ": "k", "MΩ": "M", "GΩ": "G"}.get(unit, unit)
    return f"{num}{unit}"


def _instance_symbol(sym, refdes, lcsc, x, y, inst_path, project_name):
    """A placed part symbol (rot 0, in_bom yes) with its ref/value + hidden fab fields."""
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
    return _symbol_block(sym.lib_id, x, y, 0, props, refdes, inst_path, project_name, "yes",
                         _det_uuid(project_name, "sym", refdes))


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


def _text(label, x, y, uid):
    v = label.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(text "{v}"\n\t\t(exclude_from_sim no)\n\t\t(at {x:.2f} {y:.2f} 0)'
            f"\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 2.5 2.5)\n\t\t\t\t(bold yes)"
            f"\n\t\t\t)\n\t\t\t(justify left bottom)\n\t\t)\n\t\t(uuid \"{uid}\")\n\t)\n")


def _label(net, x, y, ang, justify, uid):
    """LOCAL label — connects same-named labels WITHIN one sheet."""
    v = net.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(label "{v}"\n\t\t(at {x:.4f} {y:.4f} {ang})\n\t\t(effects\n'
            f"\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n"
            f"\t\t\t(justify {justify} bottom)\n\t\t)\n\t\t(uuid \"{uid}\")\n\t)\n")


def _glabel(net, x, y, ang, justify, uid):
    """GLOBAL label — connects by name ACROSS child sheets."""
    v = net.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(global_label "{v}"\n\t\t(shape bidirectional)\n\t\t(at {x:.4f} {y:.4f} {ang})\n'
            f"\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n"
            f"\t\t\t(justify {justify})\n\t\t)\n\t\t(uuid \"{uid}\")\n\t)\n")


def label_emitter(net, global_nets):
    return _glabel if net in global_nets else _label


def _wire(x1, y1, x2, y2, uid):
    return (f"\t(wire\n\t\t(pts\n\t\t\t(xy {x1:.4f} {y1:.4f}) (xy {x2:.4f} {y2:.4f})\n\t\t)\n"
            f"\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n"
            f"\t\t(uuid \"{uid}\")\n\t)\n")


def _no_connect(x, y, uid):
    return (f"\t(no_connect\n\t\t(at {x:.4f} {y:.4f})\n\t\t(uuid \"{uid}\")\n\t)\n")


def load_power_defs(cli, nets, ground_nets, with_flag=False):
    """Embed-ready power-symbol defs for `nets`: ground nets use stock power:GND, the rest clone
    power:+3V3 renamed to the net; with_flag also embeds power:PWR_FLAG. Returns {lib_id: def}."""
    src = Path(cli).parent.parent / "SharedSupport/symbols/power.kicad_sym"
    tmp = Path(tempfile.mkdtemp(prefix="pwr_"))
    dst = tmp / "power.kicad_sym"
    shutil.copy(src, dst)
    subprocess.run([cli, "sym", "upgrade", "--force", str(dst)], capture_output=True, text=True)
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
    for net in sorted(set(nets)):        # sorted -> deterministic lib_symbols order
        if net in ground_nets:
            out["power:GND"] = gnd.replace('(symbol "GND"', '(symbol "power:GND"', 1)
        else:
            d = rail.replace("+3V3", net)
            d = d.replace('(symbol "%s"' % net, '(symbol "power:%s"' % net, 1)
            out["power:%s" % net] = d
    return out


def _power_inst(net, x, y, rot, ref, inst_path, project_name, uid, ground=False, outdir=(0, 1)):
    """A power symbol (GND or a rail) at (x,y,rot); its net-name Value sits beyond the arrow, outward."""
    lib = "power:GND" if ground else "power:%s" % net
    val = "GND" if ground else net
    ox, oy = outdir
    lx, ly = x + ox * 3.81, y + oy * 3.81
    just = "right" if ox < 0 else "left" if ox > 0 else None   # KiCad has no 'center'
    props = (_prop("Reference", ref, x, y, hide=True)
             + _prop("Value", val, lx, ly, ang=horiz_text_ang(rot), just=just))
    return _symbol_block(lib, x, y, rot, props, ref, inst_path, project_name, "no", uid)


def _flag_inst(x, y, ref, inst_path, project_name, uid):
    """A PWR_FLAG at (x,y): its power-output pin marks the wired net DRIVEN for ERC."""
    props = (_prop("Reference", ref, x, y, hide=True)
             + _prop("Value", "PWR_FLAG", x, y - 2.54, hide=True))
    return _symbol_block("power:PWR_FLAG", x, y, 0, props, ref, inst_path, project_name, "no", uid)


# --------------------------------------------------------------------------- #
# Nets CSV (parsed ONCE): refdes, pin, net, kind (+ ignored note)
# --------------------------------------------------------------------------- #
class Nets:
    """The nets CSV parsed once. net blank = intentional no-connect; TODO_* = unfilled (fails the gate,
    left bare so ERC also flags it). kind (gnd|pwr|sig, blank=sig) is per-net, must agree across a net's
    rows, and is the source of truth for which nets are power/ground (never guessed from the name)."""

    def __init__(self, path):
        self.path = Path(path)
        self.pins_by_net = {}   # net -> [(refdes, pin)]  (assigned, CSV order)
        self.want = {}          # (refdes, pin) -> net
        self.kind = {}          # net -> gnd|pwr|sig
        self.nc = []            # (refdes, pin)  blank net = intentional no-connect
        self.todo = []          # (refdes, pin)  TODO_* = unfilled (gate failure)
        with open(path, newline="", encoding="utf-8") as f:
            rd = csvmod.DictReader(f)
            missing = {"refdes", "pin", "net"} - set(rd.fieldnames or [])
            if missing:
                sys.exit(f"ERROR: {self.path.name} missing required column(s): "
                         f"{', '.join(sorted(missing))} (header: {rd.fieldnames})")
            for r in rd:
                key = (r["refdes"], r["pin"])
                net = (r.get("net") or "").strip()
                if net.startswith("TODO"):
                    self.todo.append(key); continue
                if not net:
                    self.nc.append(key); continue
                self.pins_by_net.setdefault(net, []).append(key)
                self.want[key] = net
                k = (r.get("kind") or "").strip().lower() or "sig"
                if net in self.kind and self.kind[net] != k:
                    sys.exit(f"ERROR: net '{net}' has inconsistent 'kind' in {self.path.name} "
                             f"({self.kind[net]} vs {k}); every row of a net must agree.")
                self.kind[net] = k

    def is_ground(self, net):
        return self.kind.get(net) == "gnd"

    def is_power(self, net):
        return self.kind.get(net) in ("gnd", "pwr")

    def flag_nets(self):
        """Power/ground nets — each gets one PWR_FLAG driver (to satisfy ERC 'power not driven')."""
        return {n for n, k in self.kind.items() if k in ("gnd", "pwr")}

    def global_nets(self, subcircuit):
        """Nets whose pins span >1 subcircuit — need GLOBAL labels to cross child sheets. A net entirely
        in one subcircuit stays LOCAL. `subcircuit` = {refdes: subcircuit}; an absent refdes is its own
        singleton group (never folded with others, which would hide a real span)."""
        seen = {}
        for net, pins in self.pins_by_net.items():
            for ref, _pin in pins:
                seen.setdefault(net, set()).add(subcircuit.get(ref) or ("\x00" + ref))
        return {n for n, s in seen.items() if len(s) > 1}


# --------------------------------------------------------------------------- #
# Pin geometry
# --------------------------------------------------------------------------- #
def parse_pins(symbol):
    """(number, name, lx, ly, angle) for each pin in the symbol def block."""
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


def pin_sheet_xy(px, py, rot, lx, ly):
    """Lib pin (lx,ly) -> sheet coords for a symbol at (px,py) rotation `rot`. Verified vs KiCad ERC:
    at rot 0 sheet = (px+lx, py-ly); other rotations rotate the (lx,-ly) vector clockwise on screen."""
    dx, dy = lx, -ly                      # lib Y-up -> sheet Y-down
    if rot == 90:
        dx, dy = dy, -dx
    elif rot == 180:
        dx, dy = -dx, -dy
    elif rot == 270:
        dx, dy = -dy, dx
    return px + dx, py + dy


def _sheet_outward(ang, rot):
    """Pin outward direction in SHEET coords, composing the pin's lib angle with the instance rotation
    (same transform pin_sheet_xy applies to the position). Correct at any rotation."""
    a = math.radians(ang)
    ox, oy = round(-math.cos(a)), round(math.sin(a))       # rot-0 sheet outward
    if rot == 90:
        ox, oy = oy, -ox
    elif rot == 180:
        ox, oy = -ox, -oy
    elif rot == 270:
        ox, oy = -oy, ox
    return ox, oy


def _pt_on(p, s, eps=0.05):
    px, py = p
    x1, y1, x2, y2 = s
    if abs(y1 - y2) < eps:
        return abs(py - y1) < eps and min(x1, x2) - eps <= px <= max(x1, x2) + eps
    return abs(px - x1) < eps and min(y1, y2) - eps <= py <= max(y1, y2) + eps


# SHEET-outward direction (ox,oy) -> (label angle, horizontal justify) so text reads OUTWARD from the
# body (a pin facing left (-1,0) gets right-justified text extending left, etc). Correct at any rotation.
_OUTDIR_LABEL = {(-1, 0): (0, "right"), (1, 0): (0, "left"),
                 (0, 1): (90, "right"), (0, -1): (90, "left")}
# outward sheet direction -> power-symbol instance rotation so the glyph points AWAY from the part
_GND_ROT = {(0, 1): 0, (0, -1): 180, (1, 0): 90, (-1, 0): 270}
_RAIL_ROT = {(0, -1): 0, (0, 1): 180, (1, 0): 270, (-1, 0): 90}


# --------------------------------------------------------------------------- #
# One-pass build: place (label-aware) + wire each subcircuit sheet
# --------------------------------------------------------------------------- #
class _SheetWirer:
    """Wires ONE sheet: per pin, a power symbol (gnd/pwr), a net label (global if the net spans sheets),
    a no-connect, or a PWR_FLAG. Connectivity is by NAME — no routed wires. `insts` = the sheet's parts."""

    def __init__(self, insts, nets, by_libid, project_name, pwr_path, global_nets):
        self.nets, self.pn, self.global_nets = nets, project_name, global_nets
        self.pwr_path = pwr_path
        self.elems = []
        self.cnt = {"labels": 0, "wires": 0, "power": 0, "nc": 0}
        self._pwr_n = 0
        self.power_used = set()
        self.pinxy = {}                # (refdes, pin) -> (px,py,rot,lx,ly,ang)
        for refdes, lib_id, px, py, rot in insts:
            sym = by_libid.get(lib_id)
            if not sym:
                continue
            for num, _name, lx, ly, ang in parse_pins(sym):
                self.pinxy[(refdes, num)] = (px, py, rot, lx, ly, ang)
        self.allpins = {(round(x, 2), round(y, 2))
                        for g in self.pinxy.values()
                        for x, y in [pin_sheet_xy(g[0], g[1], g[2], g[3], g[4])]}

    def _stub(self, ref, pin):
        """(tip, stub_end, outward): a short stub in the pin's outward dir, or (tip, tip, out) if blocked."""
        px, py, rot, lx, ly, ang = self.pinxy[(ref, pin)]
        tx, ty = pin_sheet_xy(px, py, rot, lx, ly)
        tx, ty = round(tx, 2), round(ty, 2)
        ox, oy = _sheet_outward(ang, rot)
        sx, sy = round(tx + STUB * ox, 2), round(ty + STUB * oy, 2)
        clear = ((sx, sy) not in self.allpins and
                 not any(p != (tx, ty) and _pt_on(p, (tx, ty, sx, sy)) for p in self.allpins))
        return (tx, ty), ((sx, sy) if clear else (tx, ty)), (ox, oy)

    def label(self, ref, pin, net):
        if (ref, pin) not in self.pinxy:
            return
        (tx, ty), (ax, ay), (ox, oy) = self._stub(ref, pin)
        if (ax, ay) != (tx, ty):
            self.elems.append(_wire(tx, ty, ax, ay, _det_uuid(self.pn, "w", ref, pin))); self.cnt["wires"] += 1
        ang, just = _OUTDIR_LABEL.get((ox, oy), (0, "left"))
        self.elems.append(label_emitter(net, self.global_nets)(net, ax, ay, ang, just,
                                                               _det_uuid(self.pn, "lbl", ref, pin)))
        self.cnt["labels"] += 1

    def power(self, ref, pin, net):
        if (ref, pin) not in self.pinxy:
            return
        (tx, ty), (ax, ay), (ox, oy) = self._stub(ref, pin)
        if (ax, ay) != (tx, ty):
            self.elems.append(_wire(tx, ty, ax, ay, _det_uuid(self.pn, "w", ref, pin))); self.cnt["wires"] += 1
        gnd = self.nets.is_ground(net)
        rot = (_GND_ROT if gnd else _RAIL_ROT).get((ox, oy), 0)
        self._pwr_n += 1
        self.elems.append(_power_inst(net, ax, ay, rot, "#PWR%04d" % self._pwr_n, self.pwr_path,
                                      self.pn, _det_uuid(self.pn, "pwr", ref, pin),
                                      ground=gnd, outdir=(ox, oy)))
        self.power_used.add(net); self.cnt["power"] += 1

    def nc(self, ref, pin):
        g = self.pinxy.get((ref, pin))
        if not g:
            return
        tx, ty = pin_sheet_xy(g[0], g[1], g[2], g[3], g[4])
        self.elems.append(_no_connect(tx, ty, _det_uuid(self.pn, "nc", ref, pin))); self.cnt["nc"] += 1

    def flag(self, net, x, y):
        """A PWR_FLAG wired to a GLOBAL label of the net name (name-based, geometry-free driver)."""
        self._pwr_n += 1
        self.elems.append(_flag_inst(x, y, "#FLG%04d" % self._pwr_n, self.pwr_path, self.pn,
                                     _det_uuid(self.pn, "flg", net)))
        self.elems.append(_wire(x, y, x + 5.08, y, _det_uuid(self.pn, "flgw", net)))
        self.elems.append(_glabel(net, x + 5.08, y, 0, "left", _det_uuid(self.pn, "flgl", net)))


def _part_extent(sym, refdes, nets):
    """Outward reach (L,R,U,D) mm that this part's labels/power symbols add beyond its body at rot 0, so
    packing reserves room for label TEXT (conservative over-estimate). Pins with no net add nothing."""
    L = R = U = D = 0.0
    for num, _name, lx, ly, ang in parse_pins(sym):
        net = nets.want.get((refdes, num))
        if not net:
            continue
        text = "GND" if nets.is_ground(net) else net
        reach = STUB + (3.81 if nets.is_power(net) else 0.0) + len(text) * LBL_ADV + LBL_PAD  # pwr: +value offset
        ox, oy = _sheet_outward(ang, 0)
        if ox < 0:
            L = max(L, reach)
        elif ox > 0:
            R = max(R, reach)
        if oy < 0:
            U = max(U, reach)
        elif oy > 0:
            D = max(D, reach)
    return L, R, U, D


def _pack_group(parts, nets, inst_path, project_name):
    """Shelf-pack a subcircuit's parts on their label-inclusive bboxes (body + _part_extent), 50-mil grid.
    Returns (placed=[(sym,refdes,lcsc,ox,oy)], used, width, height)."""
    placed, used = [], {}
    shelf_x, shelf_top, shelf_h, max_x = MARGIN, MARGIN + HEADER_GAP, 0.0, MARGIN
    for sym, refdes, lcsc in parts:
        L, R, U, D = _part_extent(sym, refdes, nets)
        w = (sym.lmaxx - sym.lminx) + L + R + GAP        # cluster width (body + labels) + margin
        h = (sym.lmaxy - sym.lminy) + U + D + GAP
        if shelf_x > MARGIN and shelf_x + w > SHEET_W:   # wrap to next shelf
            shelf_top += shelf_h + GAP
            shelf_x, shelf_h = MARGIN, 0.0
        # origin so the LEFT/UP label reach clears the shelf edge (snap 50-mil)
        ox = round((shelf_x + GAP / 2 + L - sym.lminx) / 1.27) * 1.27
        oy = round((shelf_top + GAP / 2 + U + sym.lmaxy) / 1.27) * 1.27
        placed.append((sym, refdes, lcsc, ox, oy))
        used[sym.lib_id] = sym
        max_x = max(max_x, shelf_x + w)
        shelf_x += w
        shelf_h = max(shelf_h, h)
    width = min(max(round(max_x + MARGIN), 297), 5000)
    height = min(max(round(shelf_top + shelf_h + MARGIN), 210), 5000)
    return placed, used, width, height


def build(resolved, nets, by_libid, project_name, cli):
    """Group parts by subcircuit, then per group PLACE (label-aware pack) + WIRE (labels/power/NC; the
    PWR_FLAGs go on the first sheet) and assemble the child; then the root. Returns
    (root_text, {fname: text}, sheets, root_uuid, n_parts, n_defs, unresolved)."""
    root_uuid = _det_uuid(project_name, "root")

    order, groups, unresolved = [], {}, []
    for r in resolved:
        sym = by_libid.get(r["symbol"])
        if not sym:
            unresolved.append(r)                 # symbol not in the library
            continue
        grp = r["subcircuit"]
        if grp not in groups:
            groups[grp] = []; order.append(grp)
        groups[grp].append((sym, r["refdes"], r["lcsc"]))

    subcircuit = {r["refdes"]: r["subcircuit"] for r in resolved}
    global_nets = nets.global_nets(subcircuit)
    flag_nets = nets.flag_nets()
    flag_host = order[0] if order else None       # all PWR_FLAGs on the first subcircuit's sheet

    children, sheets, used_total, n_parts = {}, [], {}, 0
    for grp in order:
        sheet_uuid = _det_uuid(project_name, "sheet", grp)
        file_uuid = _det_uuid(project_name, "file", grp)
        inst_path = f"/{root_uuid}/{sheet_uuid}"

        placed, used, cw, ch = _pack_group(groups[grp], nets, inst_path, project_name)
        n_parts += len(placed)
        used_total.update(used)
        sym_elems = [_instance_symbol(s, ref, lcsc, ox, oy, inst_path, project_name)
                     for s, ref, lcsc, ox, oy in placed]

        insts = [(ref, s.lib_id, ox, oy, 0) for s, ref, lcsc, ox, oy in placed]
        w = _SheetWirer(insts, nets, by_libid, project_name, inst_path, global_nets)
        for net, pins in nets.pins_by_net.items():   # wirer no-ops for pins not on this sheet
            emit = w.power if nets.is_power(net) else w.label
            for ref, pin in pins:
                emit(ref, pin, net)
        for ref, pin in nets.nc:            # blank net = intentional no-connect (TODO_* left bare)
            w.nc(ref, pin)
        host_flags = flag_nets if grp == flag_host else frozenset()
        if host_flags:
            fx = 12.7
            for net in sorted(host_flags):
                w.flag(net, fx, 6.35)
                fx += 20.32
            cw = min(max(cw, round(fx + MARGIN)), 5000)   # widen the page for the PWR_FLAG row

        lib = "".join(f"\t\t{d.embedded_def()}\n" for d in used.values())
        if w.power_used or host_flags:               # embed the power defs this sheet used
            pdefs = load_power_defs(cli, w.power_used,
                                    {n for n in w.power_used if nets.is_ground(n)},
                                    with_flag=bool(host_flags))
            lib += "".join(f"\t\t{d}\n" for d in pdefs.values())

        title = _text(grp, MARGIN, MARGIN, _det_uuid(project_name, "title", grp))
        children[f"{SHEET_DIR}/{project_name}-{sheet_slug(grp)}.kicad_sch"] = (
            f'(kicad_sch\n\t(version 20260306)\n\t(generator "generate_schematic")\n'
            f'\t(generator_version "10.0")\n\t(uuid "{file_uuid}")\n'
            f'\t(paper "User" {cw} {ch})\n\t(lib_symbols\n{lib}\t)\n'
            f"{title}{''.join(sym_elems)}{''.join(w.elems)}\t(embedded_fonts no)\n)\n")
        sheets.append((grp, f"{SHEET_DIR}/{project_name}-{sheet_slug(grp)}.kicad_sch", sheet_uuid))

    # root: a grid of sheet blocks, one per child
    COLS = 6
    BOXW, BOXH, GX, GY = 45.0, 30.0, 20.0, 25.0
    rbody = []
    for i, (nm, fname, suid) in enumerate(sheets):
        col, rowi = i % COLS, i // COLS
        x = MARGIN + col * (BOXW + GX)
        yy = MARGIN + HEADER_GAP + rowi * (BOXH + GY)
        rbody.append(_sheet_block(nm, fname, suid, x, yy, BOXW, BOXH, i + 2, root_uuid, project_name))
    nrows = (len(sheets) + COLS - 1) // COLS
    rw = min(max(round(2 * MARGIN + COLS * (BOXW + GX)), 297), 5000)
    rh = min(max(round(2 * MARGIN + HEADER_GAP + nrows * (BOXH + GY)), 210), 5000)
    root = (f'(kicad_sch\n\t(version 20260306)\n\t(generator "generate_schematic")\n'
            f'\t(generator_version "10.0")\n\t(uuid "{root_uuid}")\n'
            f'\t(paper "User" {rw} {rh})\n\t(lib_symbols\n\t)\n'
            f"{''.join(rbody)}\t(sheet_instances\n\t\t(path \"/\"\n\t\t\t(page \"1\")"
            f"\n\t\t)\n\t)\n\t(embedded_fonts no)\n)\n")
    return root, children, sheets, root_uuid, n_parts, len(used_total), unresolved


def write_lib_tables(project_dir, symbols_dir):
    """Ensure the project's sym-/fp-lib-table reference our libs (create or append; idempotent)."""
    rel = os.path.relpath(symbols_dir, project_dir)

    def merge(fname, root_tag, entries):
        path = project_dir / fname
        if not path.exists():
            path.write_text(f"({root_tag}\n\t(version 7)\n" + "\n".join(entries) + "\n)\n")
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
           f'(uri "${{KIPRJMOD}}/{rel}/{n}.kicad_sym")(options "")(descr ""))' for n in LIBS]
    fp = [f'\t(lib (name "{n}")(type "KiCad")'
          f'(uri "${{KIPRJMOD}}/{rel}/{n}.pretty")(options "")(descr ""))' for n in LIBS]
    return (merge("sym-lib-table", "sym_lib_table", sym),
            merge("fp-lib-table", "fp_lib_table", fp))


def generate(cli, by_libid, project, symbols, bom, nets, only):
    """Read the BOM, build every child + the root, write/canonicalize them, sync the .kicad_pro page list
    + lib tables. `only` (list|None) rebuilds just those subcircuit sheets (deterministic UUIDs keep the
    root valid) and refuses if the sheet SET would change."""
    project_dir = project.resolve()
    pro = next(project_dir.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project_dir}")
    project_name = pro.stem
    if not bom.exists():
        sys.exit(f"ERROR: BOM CSV not found: {bom}")

    print(f"Reading parts from {bom}...")
    resolved = read_csv(bom)
    print(f"  {len(resolved)} parts.")

    root_text, children, sheets, root_uuid, n_parts, n_defs, unresolved = build(
        resolved, nets, by_libid, project_name, cli)
    sch_path = project_dir / f"{project_name}.kicad_sch"
    if n_parts == 0:
        sys.exit(f"ERROR: no symbol resolved from {bom.name} — refusing to overwrite {sch_path.name}. "
                 "Check its symbol column.")

    (project_dir / SHEET_DIR).mkdir(exist_ok=True)
    if only:
        grp_to_fname = {grp: fname for grp, fname, _ in sheets}
        unknown = [g for g in only if g not in grp_to_fname]
        if unknown:
            sys.exit(f"ERROR: --only names are not subcircuits in {bom.name}: {unknown}\n"
                     f"  available: {', '.join(sorted(grp_to_fname))}")
        # compare only within sheets/ (stale root-level *.kicad_sch would false-positive the check)
        existing = {p.resolve() for p in (project_dir / SHEET_DIR).glob(f"{project_name}-*.kicad_sch")}
        computed = {(project_dir / f).resolve() for f in children}
        if existing != computed:
            sys.exit("ERROR: --only cannot change the SET of child sheets (that needs the root + page "
                     "list rebuilt).\n"
                     f"  would add:    {sorted(p.name for p in computed - existing)}\n"
                     f"  would remove: {sorted(p.name for p in existing - computed)}\n"
                     "  Run a full build (omit --only) instead.")
        for grp in only:
            fp = project_dir / grp_to_fname[grp]
            fp.write_text(children[grp_to_fname[grp]], encoding="utf-8")
            canonicalize_sch(cli, fp)
        print(f"Rebuilt {len(only)} sheet(s) [{', '.join(only)}] — root, {pro.name} page list, and "
              "other sheets left untouched.")
        return

    # prune stale child sheets (sheets/ AND project root, from older flat layouts), then write all
    keep = {(project_dir / f).resolve() for f in children}
    for old in list(project_dir.glob(f"{project_name}-*.kicad_sch")) + \
            list((project_dir / SHEET_DIR).glob(f"{project_name}-*.kicad_sch")):
        if old.resolve() not in keep:
            old.unlink()
    for fname, text in children.items():
        (project_dir / fname).write_text(text, encoding="utf-8")
    sch_path.write_text(root_text, encoding="utf-8")
    print(f"Wrote {sch_path.name} (root) + {len(children)} child sheets: "
          f"{n_parts} parts, {n_defs} unique symbol defs.")
    for fname in children:                       # canonicalize each child individually, then the root
        canonicalize_sch(cli, project_dir / fname)
    canonicalize_sch(cli, sch_path)

    # sync the .kicad_pro page list (root = page 1, children = 2..N) so the GUI doesn't renumber. JSON
    # load/dump, NOT a regex: KiCad writes an empty list as a single-line `"sheets": [],`.
    data = json.loads(pro.read_text(encoding="utf-8"))
    data["sheets"] = [[root_uuid, "Root"]] + [[suid, nm] for nm, _fn, suid in sheets]
    pro.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"  canonicalized all sheets; synced {pro.name} page list ({len(data['sheets'])} pages).")

    missing = [(r["refdes"], r["symbol"], "no Reference" if not s.ref_found else "no Footprint")
               for r in resolved
               if (s := by_libid.get(r["symbol"])) and (not s.ref_found or not s.footprint)]
    if missing:
        print(f"  WARNING: {len(missing)} symbol(s) missing a key property (refdes prefix guessed / "
              "footprint blank):")
        for refdes, sym, what in missing[:10]:
            print(f"    {refdes} ({sym}): {what}")
    if unresolved:
        print(f"  WARNING: {len(unresolved)} rows whose symbol is not in the library:")
        for r in unresolved:
            print(f"    {r['refdes']} ({r['note']})  symbol={r['symbol']!r}")

    s1, s2 = write_lib_tables(project_dir, symbols.resolve())
    print(f"  sym-lib-table: {s1}, fp-lib-table: {s2}")


# --------------------------------------------------------------------------- #
# VALIDATE gate (exported netlist vs the nets CSV + ERC)
# --------------------------------------------------------------------------- #
def netlist_check(cli, sch_path, nets):
    """Check the exported netlist by ELECTRICAL PARTITION, not names (KiCad prefixes local nets with the
    sheet path). Flags: a CSV pin unconnected; one CSV net split across >1 exported net; two CSV nets
    shorted onto one; an intended no-connect that picked up a net. Returns [(ref, pin, detail), ...]."""
    tmp = sch_path.parent / (sch_path.stem + "-netcheck.net")
    r = subprocess.run([cli, "sch", "export", "netlist", "--format", "kicadsexpr",
                        "-o", str(tmp), str(sch_path)], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ERROR: netlist export failed:\n" + (r.stderr or r.stdout))
    txt = tmp.read_text(encoding="utf-8")
    tmp.unlink()

    actual = {}                       # (ref, pin) -> FULL exported net name
    start = txt.find("(nets")         # guard: empty/odd design -> no (nets ...) block
    if start != -1:
        for blk in re.split(r"\n\t\t\(net\b", txt[start:]):
            nm = re.search(r'\(name "([^"]*)"', blk)
            if not nm:
                continue
            for nd in re.finditer(r'\(node\s+\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk):
                actual[(nd.group(1), nd.group(2))] = nm.group(1)

    def connected(g):
        return g is not None and not g.startswith("unconnected")

    miss = []
    csv_to_real, real_to_csv = defaultdict(set), defaultdict(set)
    for key, net in nets.want.items():
        got = actual.get(key)
        if not connected(got):                                   # (a) pin not on any real net
            miss.append((key[0], key[1], f"net '{net}': pin not connected in the exported netlist"))
            continue
        csv_to_real[net].add(got)
        real_to_csv[got].add(net)
    for net, reals in sorted(csv_to_real.items()):
        if len(reals) > 1:                                       # (b) one CSV net split across nets
            miss.append(("", "", f"net '{net}' is split across {len(reals)} separate nets {sorted(reals)}"))
    for real, csvs in sorted(real_to_csv.items()):
        if len(csvs) > 1:                                        # (c) distinct CSV nets shorted
            miss.append(("", "", f"nets {sorted(csvs)} are shorted (one exported net '{real}')"))
    for key in nets.nc:                                          # (d) NC pin picked up a connection
        if connected(actual.get(key)):
            miss.append((key[0], key[1], f"intended no-connect but landed on net '{actual[key]}'"))
    return miss


def validate_full(cli, nets, sch_path):
    """GATE: (1) no TODO_* pins; (2) ERC 0 errors + 0 unconnected-pin; (3) netlist partition matches.
    Warnings tolerated. Returns True iff all pass; prints a per-check OK/FAIL summary."""
    print("\n=== VALIDATE ===")
    ok = True

    if nets.todo:
        ok = False
        pins = [f"{r}.{p}" for r, p in nets.todo]
        print(f"  FAIL: {len(pins)} pin(s) have no net (TODO_* in {nets.path.name}): "
              + ", ".join(pins[:12]) + (" …" if len(pins) > 12 else ""))
    else:
        print(f"  OK:   every pin in {nets.path.name} has a net (or an intentional no-connect)")

    rpt = sch_path.with_name(sch_path.stem + "-erc.json")
    subprocess.run([cli, "sch", "erc", "--format", "json", "-o", str(rpt), str(sch_path)],
                   capture_output=True, text=True)
    errs = warns = unconn = 0
    try:
        data = json.loads(rpt.read_text(encoding="utf-8"))
        for sheet in data.get("sheets", []):
            for v in sheet.get("violations", []):
                t = str(v.get("type", "")).lower()          # machine key, not the prose description
                if "not_connected" in t or "unconnected" in t:
                    unconn += 1
                sev = str(v.get("severity", "")).lower()
                if sev == "error":
                    errs += 1
                elif sev == "warning":
                    warns += 1
    except Exception as e:
        ok = False
        print(f"  FAIL: could not parse ERC report ({e})")
    if errs or unconn:
        ok = False
        print(f"  FAIL: ERC — {errs} error(s), {unconn} unconnected-pin violation(s) (report: {rpt.name})")
    else:
        print(f"  OK:   ERC passes (0 errors, 0 unconnected pins; {warns} warning(s))")

    miss = netlist_check(cli, sch_path, nets)
    if miss:
        ok = False
        print(f"  FAIL: {len(miss)} netlist partition mismatch(es):")
        for ref, pin, detail in miss[:12]:
            print(f"        {(ref + '.' + pin + ': ') if ref else ''}{detail}")
    else:
        print("  OK:   every rendered net landed on its pin")

    print(f"  => {'PASS' if ok else 'FAIL'}")
    return ok


# --------------------------------------------------------------------------- #
# Front door
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bom", type=Path, required=True, help="parts CSV")
    ap.add_argument("--nets", type=Path, required=True, help="nets CSV")
    ap.add_argument("--project", type=Path, default=DEFAULT_PROJECT, help="dir containing <name>.kicad_pro")
    ap.add_argument("--symbols", type=Path, default=DEFAULT_SYMBOLS)
    ap.add_argument("--subcircuit", nargs="+", metavar="NAME", default=None,
                    help="rebuild ONLY these subcircuit child sheets, leaving every other sheet untouched.")
    args = ap.parse_args()

    for p in (args.bom, args.nets):
        if not p.exists():
            sys.exit(f"ERROR: CSV not found: {p}")
    project = args.project.resolve()
    pro = next(project.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project}")
    name = pro.stem

    # Load the symbol libraries + parse the nets CSV ONCE; share the cli, symbols, and nets throughout.
    cli = find_kicad_cli()
    print(f"Loading symbol libraries from {args.symbols} (upgrading copies)...")
    by_libid = load_symbols(args.symbols.resolve(), cli)
    print(f"  {len(by_libid)} symbols parsed.")
    nets = Nets(args.nets)

    print("=== BUILD (place + wire) ===")
    generate(cli, by_libid, project, args.symbols, args.bom, nets, args.subcircuit)

    if not validate_full(cli, nets, project / f"{name}.kicad_sch"):
        sys.exit("\nVALIDATION FAILED — the schematic is not clean (see findings above).")


if __name__ == "__main__":
    main()
