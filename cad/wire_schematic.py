#!/usr/bin/env python3
"""
wire_schematic.py — Add net-label connectivity to a placed KiCad schematic.

Connectivity is expressed with net *labels*: a label named e.g. "GND" placed on
a pin's connection point ties that pin to every other "GND" label. No wires are
routed between scattered symbols — matching label names do the connecting.

Two stages, mirroring bom_to_schematic.py's CSV-driven design:

    gb3.kicad_sch --bootstrap--> gb3-nets.csv --render--> gb3.kicad_sch (labelled)
                                      ▲
                    the net spec: (refdes, pin, pin_name, net, note)
                    — reviewable, hand-editable, ERC-checkable

The bootstrap fills ONLY what is safe/known and leaves the rest as TODO:
  * GND family (GND/PGND/VSS/EP...)          -> GND            (safe)
  * IC power pins with a BOM-cited rail       -> SYS/+3V3/+5V/BAT (see RAILS)
  * I2C bus pins (SDA/SCL) by bus membership  -> I2Cn_SDA/SCL   (see I2C_BUS)
  * a few explicit signal ties (e.g. PCM5102 SCK->GND)
  * everything else                           -> TODO_<refdes>_<pin>

Power-looking pins that are NOT simple rails are special-cased to TODO with a
warning (INA226 VBUS = sense, TPA6132 HPVDD = cap-only, CH224K VDD = post-R,
BT830 VREG_OUT_HV = LDO output). I2S is left as TODO (multidrop + series R).

The renderer never invents connectivity: it only draws labels for CSV rows
whose net is a real name (not blank / not TODO_*). Re-running strips previously
added labels first, so it is idempotent.

Usage:
    python3 wire_schematic.py                     # bootstrap + render
    python3 wire_schematic.py --bootstrap-only    # write/refresh the CSV, stop
    python3 wire_schematic.py --render-only        # render from existing CSV
    python3 wire_schematic.py --validate           # run kicad-cli sch erc after
"""

import argparse
import csv as csvmod
import math
import re
import subprocess
import sys
from pathlib import Path

import bom_to_schematic as b2s   # reuse match_paren, load_symbols, find_kicad_cli, _u

STUB = 2.54   # mm: length of the stub wire drawn from each pin out to its label

# --------------------------------------------------------------------------- #
# BOM-cited assignment tables (design-critical — each entry traces to the BOM)
# --------------------------------------------------------------------------- #
# Keyed by symbol Value -> {pin_name: (net, why)}. A rail here is asserted; a
# power pin absent from its symbol's entry falls through to TODO_rail.
RAILS = {
    "FP6161KR-LF-ADJ":  {"VIN": ("SYS", "#3 input=SYS")},
    "TPS63021DSJR":     {"VINA": ("SYS", "#66 buck-boost in=SYS"),
                         "VIN": ("SYS", "#66 buck-boost in=SYS")},
    "MAX98357AETE+T":   {"VDD": ("SYS", "#47 VDD on SYS")},
    "DRV2605LDGSR":     {"VDD": ("+3V3", "#41 VDD on 3.3V not SYS")},
    "STM6601CA2BDM6F":  {"VCC": ("SYS", "#94 VCC always-on SYS")},
    "MAX17048G+T10":    {"VDD": ("BAT", "#77 VDD->battery+")},
    "W25Q128JVSIQTR":   {"VCC": ("+3V3", "SPI-NOR 3.3V logic")},
    "A-MICROTF-1.85A":  {"VDD": ("+3V3", "microSD 3.3V")},
    "SP3004-04XTG":     {"VCC": ("+3V3", "SD-line ESD clamp ref = 3.3V")},
    "PCA9555PWR":       {"VCC": ("+3V3", "#85 i2c1 expander 3.3V logic")},
    "PCM5102APWR":      {"CPVDD": ("+3V3", "#105 3.3V"),
                         "AVDD": ("+3V3", "#105 3.3V"),
                         "DVDD": ("+3V3", "#105 DVDD strapped 3.3V")},
    "TPA6132A2RTER":    {"VDD": ("+3V3", "#110 VDD 3.3V")},
    "LSM6DSOXTR":       {"VDD": ("+3V3", "IMU 3.3V (REVIEW: rail not explicit)"),
                         "VDDIO": ("+3V3", "IMU IO 3.3V (REVIEW)")},
    "TYPE-C-31-M-12":   {"VBUS": ("+5V", "USB-C 5V input")},
    "CH224K":           {"VBUS": ("+5V", "#61 USB-C 5V rail sense")},
    "INA226AIDGSR":     {"VBUS": ("SYS", "#81 bus-voltage SENSE pin -> SYS (not a supply)")},
    "TPS61165DBVR":     {"VIN": ("SYS", "#28 VIN from SYS/5V not 3.3V (REVIEW: SYS vs +5V)")},
    "BT830-SA-01-T_R":  {"VDD_PADS": ("+3V3", "BT830 note: VDD_PADS=3.3V"),
                         "VREG_IN_HV": ("+3V3", "BT830 note: VREG_IN_HV=3.3V")},
}

# Power-looking pins that must NOT be tied to a rail -> TODO with a warning.
NO_RAIL = {
    ("TPA6132A2RTER", "HPVDD"): "cap-only charge-pump node, NEVER a supply (#110)",
    ("CH224K", "VDD"): "fed via 1k from +5V (#62) — own node, not a rail",
    ("BT830-SA-01-T_R", "VREG_OUT_HV"): "internal LDO output (needs #100 cap) — own node",
}

# I2C bus membership (by symbol Value) -> bus name.
I2C_BUS = {
    "DRV2605LDGSR": "I2C1", "MAX17048G+T10": "I2C1", "INA226AIDGSR": "I2C1",
    "PCA9555PWR": "I2C1", "LSM6DSOXTR": "I2C2",
}

# Explicit single-pin signal ties that are safe and BOM-stated.
SIGNAL_TIES = {
    ("PCM5102APWR", "SCK"): ("GND", "#103 SCK(12)->GND (MCLK-less internal PLL)"),
}

GND_RE = re.compile(r"^(A?GND|PGND|DGND|SGND|VSS\w*|EP|EPAD|PAD|GND\d*)$", re.I)
PWR_RE = re.compile(r"^(VDD\w*|VCC\w*|VIN\w*|AVDD|DVDD|CPVDD|VBAT|VBUS|VS|IOVDD|"
                    r"HPVDD|VREG\w*)$", re.I)
I2C_RE = re.compile(r"^(SDA|SCL)$", re.I)

NETS_COLUMNS = ["refdes", "pin", "pin_name", "net", "note"]


def assign_net(value, refdes, pin_name):
    """Return (net, note) for one pin. TODO_* / '' nets are not rendered."""
    todo = f"TODO_{refdes}_{pin_name}"
    if GND_RE.match(pin_name):
        return "GND", "ground family"
    if (value, pin_name) in SIGNAL_TIES:
        return SIGNAL_TIES[(value, pin_name)]
    if (value, pin_name) in NO_RAIL:
        return todo, "NO-RAIL: " + NO_RAIL[(value, pin_name)]
    if I2C_RE.match(pin_name) and value in I2C_BUS:
        return f"{I2C_BUS[value]}_{pin_name.upper()}", f"{I2C_BUS[value]} bus"
    if PWR_RE.match(pin_name):
        rails = RAILS.get(value, {})
        if pin_name in rails:
            net, why = rails[pin_name]
            return net, "rail: " + why
        return todo, "rail unknown — assign manually"
    return todo, ""


# --------------------------------------------------------------------------- #
# Pin geometry + instance parsing
# --------------------------------------------------------------------------- #
def parse_pins(symbol):
    """(number, name, lx, ly, angle) for each pin, from the symbol def block."""
    out, b = [], symbol.block
    for pm in re.finditer(r"\(pin\s", b):
        seg = b[pm.start():b2s.match_paren(b, pm.start())]
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
        blk = sch_text[m.start() + 1:b2s.match_paren(sch_text, m.start() + 1)]
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
# Stage 1: bootstrap gb3.kicad_sch -> nets CSV
# --------------------------------------------------------------------------- #
def bootstrap(sch_text, by_libid, csv_path):
    rows, stats = [], {"GND": 0, "rail": 0, "i2c": 0, "tie": 0, "todo": 0}
    for refdes, lib_id, _px, _py, _rot in parse_instances(sch_text):
        sym = by_libid.get(lib_id)
        if not sym:
            continue
        for num, name, *_ in parse_pins(sym):
            net, note = assign_net(sym.value, refdes, name or num)
            rows.append(dict(refdes=refdes, pin=num, pin_name=name,
                             net=net, note=note))
            if net == "GND":
                stats["GND"] += 1
            elif net.startswith("I2C"):
                stats["i2c"] += 1
            elif net.startswith("TODO"):
                stats["todo"] += 1
            elif note.startswith("rail"):
                stats["rail"] += 1
            else:
                stats["tie"] += 1
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csvmod.DictWriter(f, fieldnames=NETS_COLUMNS)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    return rows, stats


# --------------------------------------------------------------------------- #
# Stage 2: render nets CSV -> labels on the schematic
# --------------------------------------------------------------------------- #
def _label(net, x, y, ang):
    v = net.replace("\\", "\\\\").replace('"', '\\"')
    return (f'\t(label "{v}"\n\t\t(at {x:.4f} {y:.4f} {ang})\n\t\t(effects\n'
            f"\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n"
            f"\t\t\t(justify left bottom)\n\t\t)\n\t\t(uuid \"{b2s._u()}\")\n\t)\n")


def _wire(x1, y1, x2, y2):
    return (f"\t(wire\n\t\t(pts\n\t\t\t(xy {x1:.4f} {y1:.4f}) (xy {x2:.4f} {y2:.4f})\n\t\t)\n"
            f"\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n"
            f"\t\t(uuid \"{b2s._u()}\")\n\t)\n")


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
        end = b2s.match_paren(result, m.start() + 1)
        result = result[:m.start()] + result[end:]
    return result


def render(sch_text, csv_path, by_libid):
    """Return (new_sch_text, n_labels, skipped) for real-net CSV rows."""
    want = {}
    with open(csv_path, newline="", encoding="utf-8") as f:
        for r in csvmod.DictReader(f):
            net = r["net"].strip()
            if net and not net.startswith("TODO"):
                want[(r["refdes"], r["pin"])] = net

    sch_text = _strip(_strip(sch_text, "label"), "wire")
    insts = parse_instances(sch_text)
    if not insts:
        sys.exit("ERROR: parsed 0 symbol instances from the schematic — "
                 "read-back regex mismatch; aborting rather than emit nothing.")
    if re.search(r"\(mirror [xy]\)", sch_text):
        print("  WARNING: schematic has mirrored symbol(s); mirror is NOT applied "
              "to pin geometry — labels on mirrored parts may miss their pins.")
    elems, n_labels = [], 0
    for refdes, lib_id, px, py, rot in insts:
        sym = by_libid.get(lib_id)
        if not sym:
            continue
        for num, name, lx, ly, ang in parse_pins(sym):
            net = want.get((refdes, num))
            if not net:
                continue
            # pin tip, and a stub extending outward (body->tip direction, beyond)
            tx, ty = pin_sheet_xy(px, py, rot, lx, ly)
            ex, ey = pin_sheet_xy(px, py, rot,
                                  lx - STUB * math.cos(math.radians(ang)),
                                  ly - STUB * math.sin(math.radians(ang)))
            elems.append(_wire(tx, ty, ex, ey))
            elems.append(_label(net, ex, ey, (ang + 180) % 360))
            n_labels += 1

    ins = sch_text.find("\t(sheet_instances")
    if ins == -1:
        sys.exit("ERROR: no sheet_instances block found in schematic")
    new = sch_text[:ins] + "".join(elems) + sch_text[ins:]
    return new, n_labels, 0


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
    actual = {}
    sec = txt[txt.index("(nets"):]
    for blk in re.split(r"\n\t\t\(net\b", sec):
        nm = re.search(r'\(name "([^"]*)"', blk)
        if not nm:
            continue
        net = nm.group(1).lstrip("/")        # KiCad prefixes local-label nets
        for nd in re.finditer(r'\(node\s+\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk):
            actual[(nd.group(1), nd.group(2))] = net
    miss = []
    with open(nets_path, newline="", encoding="utf-8") as f:
        for row in csvmod.DictReader(f):
            net = row["net"].strip()
            if not net or net.startswith("TODO"):
                continue
            got = actual.get((row["refdes"], row["pin"]))
            if got != net.lstrip("/"):
                miss.append((row["refdes"], row["pin"], net, got))
    return miss


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project", type=Path, default=b2s.DEFAULT_PROJECT)
    ap.add_argument("--symbols", type=Path, default=b2s.DEFAULT_SYMBOLS)
    ap.add_argument("--nets", type=Path, default=None,
                    help="nets CSV path (default: <project>/<name>-nets.csv)")
    ap.add_argument("--bootstrap-only", action="store_true")
    ap.add_argument("--render-only", action="store_true")
    ap.add_argument("--validate", action="store_true")
    args = ap.parse_args()

    cli = b2s.find_kicad_cli()
    project_dir = args.project.resolve()
    pro = next(project_dir.glob("*.kicad_pro"), None)
    if not pro:
        sys.exit(f"ERROR: no .kicad_pro in {project_dir}")
    name = pro.stem
    sch_path = project_dir / f"{name}.kicad_sch"
    nets_path = args.nets or (project_dir / f"{name}-nets.csv")
    if not sch_path.exists():
        sys.exit(f"ERROR: {sch_path} not found (run bom_to_schematic.py first)")

    print(f"Loading symbol libraries from {args.symbols}...")
    _by_lcsc, by_libid = b2s.load_symbols(args.symbols.resolve(), cli)
    sch_text = sch_path.read_text(encoding="utf-8")

    if not args.render_only:
        print(f"Bootstrapping nets from {sch_path.name}...")
        rows, stats = bootstrap(sch_text, by_libid, nets_path)
        print(f"  {len(rows)} pins -> {nets_path.name}")
        print(f"  auto-assigned: GND={stats['GND']} rails={stats['rail']} "
              f"I2C={stats['i2c']} ties={stats['tie']} | TODO={stats['todo']}")
        if args.bootstrap_only:
            print("  (review/edit the CSV, then re-run with --render-only)")
            return

    print(f"Rendering labels from {nets_path.name}...")
    print("  NOTE: render replaces ALL labels/wires — hand-drawn connections in "
          "the schematic are not preserved.")
    new_text, n, _ = render(sch_text, nets_path, by_libid)
    sch_path.write_text(new_text, encoding="utf-8")
    b2s.canonicalize_sch(cli, sch_path)
    print(f"  {n} net labels placed. Wrote + canonicalized {sch_path.name}")

    if args.validate:
        print("Validating with kicad-cli sch erc...")
        erc_rpt = sch_path.with_name(f"{name}-erc.rpt")
        r = subprocess.run([cli, "sch", "erc", "-o", str(erc_rpt), str(sch_path)],
                           capture_output=True, text=True)
        print("  " + (r.stdout.strip() or r.stderr.strip()).replace("\n", "\n  "))
        print(f"  report: {erc_rpt}")
        miss = netlist_check(cli, sch_path, nets_path)
        if miss:
            print(f"  NETLIST MISMATCH: {len(miss)} rendered net(s) did NOT land on "
                  "the intended pin (label geometry?):")
            for ref, pin, want_net, got in miss[:15]:
                print(f"    {ref}.{pin}: want {want_net}, got {got or 'unconnected'}")
        else:
            print("  netlist check: all rendered nets landed on their pins.")


if __name__ == "__main__":
    main()
