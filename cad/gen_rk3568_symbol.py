#!/usr/bin/env python3
"""
gen_rk3568_symbol.py — build the RK3568B2 KiCad symbol straight from Rockchip's pinout workbook.

    python3 cad/gen_rk3568_symbol.py        # (re)writes manual:RK3568B2 in cad/symbols/manual.kicad_sym

Source: cad/docs/RK3568-pinout-V1.1.xlsx, sheet "05.PinOut List". Rockchip partitions the 636 balls into
Parts A..W (the split their reference schematic uses); each Part becomes one unit, in order, so unit N is
Part chr(ord('A') + N - 1). Pin number = ball, pin name = Rockchip's "Pin Name" (full mux string + reset
pull suffix), electrical type from the "Pin type" column (TYPES below). Inputs/power are drawn on the
left, outputs/bidirectionals on the right.

Why not the EasyEDA symbol (C3345225): its 7 units mix GPIO, DDR, power and VSS balls on every unit, so
no unit maps to one schematic page, and every pin is typed unspecified (ERC can't check anything).
"""
from __future__ import annotations

import html
import math
import re
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
XLSX = HERE / "docs/RK3568-pinout-V1.1.xlsx"
LIB = HERE / "symbols/manual.kicad_sym"
NAME = "RK3568B2"
FOOTPRINT = "easyeda2kicad:BGA-636_L19.0-W19.0-P0.65-BL_RTD1296PB"
SHEET = "05.PinOut List"

# Rockchip "Pin type" -> KiCad electrical type. PO (DDR_VREFOUT, the only one) is a reference output,
# typed `output` not `power_out` so a PWR_FLAG on its net can't collide with it.
TYPES = {
    "PI": "power_in", "API": "power_in", "G": "power_in", "AG": "power_in",
    "I": "input", "AI": "input", "CI": "input",
    "O": "output", "AO": "output", "CO": "output", "PO": "output",
    "I/O": "bidirectional", "AI/O": "bidirectional",
    "A": "passive",
}
LEFT = {"power_in", "input", "passive"}       # everything else goes on the right

PITCH = 2.54
PIN_LEN = 2.54
CHAR_W = 1.0          # mm per pin-name character at 1.27 mm text (deliberate over-estimate)
NAME_OFF = 1.016
MID_GAP = 5.08        # clear space between the left and right name columns
BALL = re.compile(r"[A-HJ-NPRT-WY]{1,2}\d{1,2}")


def read_sheet(path, sheet):
    """{row: {col: text}} for one sheet of an .xlsx (stdlib only: shared + inline strings)."""
    z = zipfile.ZipFile(path)
    wb = z.read("xl/workbook.xml").decode()
    sheets = dict(re.findall(r'<sheet [^>]*?name="([^"]+)"[^>]*?r:id="([^"]+)"', wb))
    rels = {}
    for m in re.finditer(r"<Relationship [^>]*>", z.read("xl/_rels/workbook.xml.rels").decode()):
        s = m.group(0)
        rels[re.search(r'Id="([^"]+)"', s).group(1)] = re.search(r'Target="([^"]+)"', s).group(1)
    shared = [html.unescape(re.sub(r"<[^>]+>", "", m))
              for m in re.findall(r"<si>(.*?)</si>", z.read("xl/sharedStrings.xml").decode(), re.S)]
    xml = z.read("xl/" + rels[sheets[sheet]].replace("/xl/", "").lstrip("/")).decode()
    out = {}
    for m in re.finditer(r'<c r="([A-Z]+)(\d+)"([^>]*?)(?:/>|>(.*?)</c>)', xml, re.S):
        col, row, attrs, body = m.group(1), int(m.group(2)), m.group(3), m.group(4) or ""
        v = re.search(r"<v>([^<]*)</v>", body)
        inline = re.search(r"<is>(.*?)</is>", body, re.S)
        if 't="s"' in attrs and v:
            val = shared[int(v.group(1))]
        elif inline:
            val = html.unescape(re.sub(r"<[^>]+>", "", inline.group(1)))
        elif v:
            val = v.group(1)
        else:
            continue
        out.setdefault(row, {})[col] = val.strip()
    return out


def read_parts():
    """[(letter, title, [(ball, name, kicad_type)])] in workbook order. A blank Pin type is a merged
    cell (the _2 duplicate of a power ball) and inherits the row above."""
    rows = read_sheet(XLSX, SHEET)
    parts, prev = [], None
    for r in sorted(rows):
        a, b = rows[r].get("A", ""), rows[r].get("B", "")
        m = re.fullmatch(r"Part ([A-Z])", a)
        if m:
            parts.append([m.group(1), b, int(rows[r].get("C") or 0), []])
            prev = None
            continue
        if not (parts and BALL.fullmatch(a) and b):
            continue
        t = rows[r].get("E", "") or prev
        if t not in TYPES:
            sys.exit(f"ERROR: ball {a} ({b}) has unknown pin type {t!r}")
        parts[-1][3].append((a, b, TYPES[t]))
        prev = t
    for letter, title, declared, pins in parts:
        if declared and declared != len(pins):
            sys.exit(f"ERROR: Part {letter} declares {declared} balls but lists {len(pins)}")
    balls = [p[0] for part in parts for p in part[3]]
    if len(balls) != 636 or len(set(balls)) != 636:
        sys.exit(f"ERROR: expected 636 unique balls, got {len(balls)} ({len(set(balls))} unique)")
    return [(letter, title, pins) for letter, title, _, pins in parts]


def snap(v):
    return math.ceil(v / PITCH - 1e-9) * PITCH


def unit_block(n, letter, title, pins):
    left = [p for p in pins if p[2] in LEFT]
    right = [p for p in pins if p[2] not in LEFT]
    if not left or not right:                    # power/ground-only Parts: split evenly, keep order
        half = (len(pins) + 1) // 2
        left, right = pins[:half], pins[half:]
    lw = max((len(p[1]) for p in left), default=0) * CHAR_W + NAME_OFF
    rw = max((len(p[1]) for p in right), default=0) * CHAR_W + NAME_OFF
    half_w = snap(max(lw, rw, len(title) * CHAR_W / 2) + MID_GAP / 2)   # symmetric body, pins on grid
    rows = max(len(left), len(right))
    # every unit hangs from the same top edge, so the symbol-wide Reference/Value (whose positions are
    # shared by all units) sit just above any unit's body
    top = 0.0
    body_top, body_bot = top + 2 * PITCH, top - rows * PITCH        # 2 rows of headroom for the title
    out = [f'    (symbol "{NAME}_{n}_1"',
           f"      (rectangle (start {-half_w:.2f} {body_top:.2f}) (end {half_w:.2f} {body_bot:.2f})",
           "        (stroke (width 0.254)) (fill (type background))",
           "      )",
           f'      (text "Part {letter}: {title}" (at 0 {top + PITCH:.2f} 0)',
           "        (effects (font (size 1.27 1.27)))",
           "      )"]
    for side, col in ((-1, left), (1, right)):
        x = side * (half_w + PIN_LEN)
        ang = 0 if side < 0 else 180
        for i, (ball, name, etype) in enumerate(col):
            out += [f"      (pin {etype} line (at {x:.2f} {top - i * PITCH:.2f} {ang}) (length {PIN_LEN})",
                    f'        (name "{name}" (effects (font (size 1.27 1.27))))',
                    f'        (number "{ball}" (effects (font (size 1.27 1.27))))',
                    "      )"]
    out.append("    )")
    return "\n".join(out)


def prop(key, value, idnum, y=0.0, hide=True):
    h = " hide" if hide else ""
    return (f'    (property "{key}" "{value}" (id {idnum}) (at 0 {y:.2f} 0)\n'
            f"      (effects (font (size 1.27 1.27)){h})\n    )")


def symbol_text(parts):
    desc = ("Rockchip RK3568B2 quad Cortex-A55 SoC, FCBGA-636 19x19 mm 0.65 mm. Units 1-23 = Rockchip "
            "pinout Parts A-W. Generated by cad/gen_rk3568_symbol.py from RK3568-pinout-V1.1.xlsx")
    head = [f'  (symbol "{NAME}" (pin_names (offset {NAME_OFF})) (in_bom yes) (on_board yes)',
            prop("Reference", "U", 0, 2 * PITCH + 3.81, hide=False),
            prop("Value", NAME, 1, 2 * PITCH + 1.27, hide=False),
            prop("Footprint", FOOTPRINT, 2),
            prop("Datasheet", "cad/docs/RK3568-datasheet-V1.2.pdf", 3),
            prop("ki_locked", "", 4),
            prop("ki_description", desc, 5),
            prop("LCSC Part", "C3345225", 6)]
    units = [unit_block(n, *part) for n, part in enumerate(parts, 1)]
    return "\n".join(head + units + ["  )"])


def write_lib(text):
    """Replace the existing RK3568B2 block in the library, or append it before the final paren."""
    lib = LIB.read_text(encoding="utf-8")
    start = lib.find(f'\n  (symbol "{NAME}"')
    if start != -1:
        nxt = lib.find('\n  (symbol "', start + 1)
        end = nxt if nxt != -1 else lib.rindex("\n)")
        lib = lib[:start] + "\n" + text + lib[end:]
    else:
        end = lib.rindex("\n)")
        lib = lib[:end] + "\n" + text + lib[end:]
    LIB.write_text(lib, encoding="utf-8")


def main():
    parts = read_parts()
    write_lib(symbol_text(parts))
    print(f"Wrote manual:{NAME} to {LIB.relative_to(HERE.parent)}: "
          f"{sum(len(p[2]) for p in parts)} pins in {len(parts)} units")
    for n, (letter, title, pins) in enumerate(parts, 1):
        print(f"  unit {n:2} = Part {letter}: {title} ({len(pins)})")


if __name__ == "__main__":
    main()
