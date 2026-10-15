"""gb2 rev B electrical floor plan: two variants (T = panel taped to the board, F = panel floating in the front shell).

Read-only inputs: enclosure/build/board.json (rev A parts), /tmp/revb/plan/outlines.json (rev B outline from the placement study).
Writes: ee_plan.json (zones, blocks, keep-outs, routes, checks) and the PNGs next to this file.
Run:  enclosure/.venv/bin/python ee_floorplan.py
Coordinates: KiCad board mm, +Y down, seen from the FRONT for both sides (back is NOT mirrored).
"""
import json, math, os
from shapely.geometry import Polygon, box, Point, LineString
from shapely.ops import unary_union

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
BOARD = json.load(open(os.path.join(ROOT, 'enclosure/build/board.json')))
OUT = json.load(open('/tmp/revb/plan/outlines.json'))['revB']          # ring 0 = edge, rings 1-3 = shell-screw holes
EDGE = Polygon(OUT[0]); HOLES = [Polygon(r) for r in OUT[1:]]
PARTS = {p['ref']: p for p in BOARD['parts']}

# ---------------------------------------------------------------- shared mechanical facts (REVB_DESIGN.md §3)
PANEL = (63.25, 45.90, 132.55, 96.10)            # MI0283QT-11 module, active area centred on (95.25, 71.0)
ACTIVE = (66.45, 49.40, 124.05, 92.60)
ANT = (25.2, 38.6, 34.9, 52.4)                   # BT830 antenna keep-out, all layers (rev A zone)
SCREWS = [(57.39, 35.47), (133.19, 35.45), (29.24, 91.91)]
SPEAKER = (151.45, 85.1, 13.5)
LRA = (28.5, 82.0, 4.0)
POSTS = [(39.95, 56.09), (39.95, 72.09), (31.95, 64.09), (47.95, 64.09), (49.49, 83.82), (49.44, 91.82),
         (156.88, 61.92), (143.94, 66.25)]       # back-shell posts under the 8 face keys (t = 1.0)
DPAD_PIVOT = (39.95, 64.09, 5.3)

# rev A footprints that are re-placed in rev B (the core, the display sockets, test pads, J1/J8, NTC ...)
MOVED = set("""U1 C1 C2 C3 C4 C5 C6 C7 C8 C9 C10 FB1 R1 Y1 C11 C12 R3 C45 R26 R27 R13 R14 U2 C13 C14 C15 C16 C17 C18 C19
C20 FB2 R4 R5 R6 R7 D2 R29 U3 C21 C22 C23 TP7 U4 C24 R8 U7 C31 C32 R12 J3 J4 J1 J8 R20 C25 C26 C51 R31 TP1 TP2 TP3
TP4 TP5 TP6 TP8 TP9 TP10 TP11 TP12 R19""".split())

def R(x0, y0, x1, y1): return box(min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))
def circ(x, y, r): return Point(x, y).buffer(r, 48)

# Functional zone categories -> palette slot (fixed order, dataviz reference palette, light mode)
CAT = {'CORE': '#2a78d6', 'POWER': '#eb6834', 'RF': '#1baf7a', 'DISPLAY': '#eda100', 'AUDIO': '#e87ba4',
       'BATTERY': '#008300', 'HMI': '#4a3aa7', 'DEBUG': '#e34948'}

# ---------------------------------------------------------------- variant T: panel TAPED to the board front
T = dict(
    key='T', title='Variant T - panel taped flat to the board (FPC through a board opening, J3 on the back)',
    opening=(118.25, 47.55, 133.05, 94.45),
    cell=(65.6, 40.0, 104.1, 102.0), bay=(62.8, 37.0, 120.7, 108.0),
    zones=[  # broad functional zones (drawn translucent); side F/B
        ('F', 'POWER', 'F-PWR  power entry: USB-C, charger,\nQ1, buck-boost, LCD boost (rev A, kept)', (60.5, 32.6, 112.9, 45.4)),
        ('F', 'HMI', 'F-HMI-L  D-pad, Start/Select, L,\nexpander U11, haptics U8 + LRA', (22.6, 33.5, 62.9, 106.0)),
        ('F', 'AUDIO', 'F-AUD  amp U6, speaker, J5\n+ A/B, Vol, R keys', (136.5, 34.5, 167.2, 104.0)),
        ('F', 'CORE', 'F-NOR/IMU', (113.2, 33.5, 129.0, 43.0)),
        ('B', 'RF', 'B-RF  BT830 corner', (22.6, 36.0, 38.5, 62.0)),
        ('B', 'CORE', 'B-CORE-R  MCU hub', (134.5, 36.0, 160.0, 59.0)),
        ('B', 'CORE', 'B-CORE-T  FPGA + LDO', (113.0, 32.8, 133.8, 47.2)),
        ('B', 'DISPLAY', 'B-DISP  J3 + backlight', (104.7, 46.8, 118.2, 86.5)),
        ('B', 'DEBUG', 'B-BAY  test pads (door open)', (66.0, 102.7, 118.0, 107.3)),
    ],
    blocks=[  # (side, cat, name, members, box)
        ('B', 'CORE', 'MCU', 'U1 + decap ring C1-C10, FB1, R1, Y1/C11/C12/R3, R13/R14 (I2C pull-ups), R26/R27/C45 (BAT sense)', (136.0, 38.5, 158.0, 58.5)),
        ('B', 'CORE', 'FPGA', 'U2 + ring C13-C20, FB2, R4-R7, D2, R29 (WR 33R at the pin), R38', (113.5, 33.2, 128.5, 47.0)),
        ('B', 'CORE', 'LDO', 'U3 + C21-C23 (+1V2), TP7', (129.0, 39.0, 133.4, 46.8)),
        ('F', 'CORE', 'NOR', 'U4 W25Q128 (SOIC-8 ok, 2.07 < 4.4) + C24, R8', (120.4, 33.8, 128.6, 42.6)),
        ('F', 'CORE', 'IMU', 'U7 + C31, C32, R12 (13.8 mm from the screw)', (113.4, 35.0, 119.4, 42.0)),
        ('B', 'BATTERY', 'J8', 'J8 PH-2 (5.5 tall, in the bay) directly behind U9: BAT trunk ~5 mm', (104.9, 37.3, 112.9, 45.0)),
        ('B', 'DISPLAY', 'BL', 'backlight: R32-R35, Q2, R36, R37, C25, C26 by J3 pins 1-5', (104.9, 47.0, 112.2, 55.2)),
        ('B', 'DISPLAY', 'J3', 'AFC07-S50ECA-00 top contact (fixed by the FPC joggle)', (105.4, 55.66, 112.75, 86.34)),
        ('B', 'DISPLAY', 'CAPS', 'C52 10u / C53 100n for VCI/VDDI (pins 40-42)', (104.9, 86.7, 110.6, 88.8)),
        ('B', 'BATTERY', 'NTC', 'R20 at the cell edge, 56 mm from U9', (104.7, 95.0, 106.5, 96.3)),
        ('B', 'DEBUG', 'SWD', 'Tag-Connect TC2030-NL + TP8 BOOT0, TP11/12 console, next to the MCU', (137.5, 59.0, 142.0, 63.4)),
        ('B', 'DEBUG', 'TP', 'rail pads VBUS BAT SYS +3V3 +5V_LCD +1V2 GND x2, 2.54 pitch', (70.0, 103.0, 92.0, 106.0)),
        ('F', 'POWER', 'ESD', 'VBUS TVS + USB D+/D- ESD at J7 (new)', (88.0, 41.5, 92.5, 44.6)),
    ],
    keepouts=[  # (side, kind, label, geometry)
        ('F', 'zero', 'under panel: no parts, masked copper only', R(62.95, 45.60, 133.85, 96.40)),
        ('A', 'hole', 'FPC opening (through) + 0.5', R(117.75, 47.05, 133.55, 94.95)),
        ('B', 'cell', 'cell: no parts', R(65.1, 39.5, 104.6, 102.5)),
        ('B', 'zero', 'FPC sweep', R(111.48, 57.25, 118.25, 84.75)),
    ],
)

# ---------------------------------------------------------------- variant F: panel FLOATING in the front shell (2.3 mm gap)
F = dict(
    key='F', title='Variant F - panel floating 2.3 mm above the board (core under the panel, J3 on the front, no opening)',
    opening=None,
    cell=(76.0, 40.0, 114.5, 102.0), bay=(62.8, 37.0, 123.8, 108.0),
    zones=[
        ('F', 'POWER', 'F-PWR  power entry (rev A, kept)', (60.5, 32.6, 112.9, 45.4)),
        ('F', 'HMI', 'F-HMI-L  D-pad, Start/Select, L,\nexpander U11, haptics U8 + LRA', (22.6, 33.5, 62.9, 106.0)),
        ('F', 'AUDIO', 'F-AUD  amp U6, speaker, J5\n+ A/B, Vol, R keys', (136.5, 34.5, 167.2, 104.0)),
        ('F', 'CORE', 'F-CORE (under panel, <= 2.0 mm)', (64.0, 46.5, 104.6, 83.5)),
        ('F', 'DISPLAY', '', (96.5, 50.0, 111.7, 88.9)),
        ('B', 'RF', 'B-RF  BT830 corner', (22.6, 36.0, 38.5, 62.0)),
        ('B', 'DEBUG', 'B-BAY-L  SWD + pads\n(door open)', (63.0, 46.0, 75.4, 74.0)),
    ],
    blocks=[
        ('F', 'CORE', 'MCU', 'U1 + ring, Y1/C11/C12/R3, FB1, R1, R13/R14, R26/R27/C45', (74.0, 47.0, 96.0, 67.0)),
        ('F', 'CORE', 'FPGA', 'U2 + ring, FB2, R4-R7, D2, R29, R38; bus side faces J3', (88.0, 68.5, 102.5, 82.3)),
        ('F', 'CORE', 'LDO', 'U3 + C21-C23, TP7; +1V2 island on L3 under U2', (82.5, 70.0, 87.0, 77.7)),
        ('F', 'CORE', 'NOR', 'U4 as WSON-8 6x5 (0.80 max) + C24, R8', (65.5, 69.0, 73.5, 77.0)),
        ('F', 'CORE', 'IMU', 'U7 + C31, C32, R12 at the board centre (no posts/screws near)', (75.0, 70.0, 81.0, 76.0)),
        ('F', 'DISPLAY', 'BL', 'backlight R32-R37, Q2, C25, C26 by J3 pins 1-5', (97.0, 50.5, 104.3, 57.9)),
        ('F', 'DISPLAY', 'J3', 'bottom-contact 50p 0.5 mm (C262574) or dual-contact AFC24 (C262687), <= 2.0 tall', (104.65, 55.66, 111.65, 86.34)),
        ('F', 'DISPLAY', 'CAPS', 'C52/C53 at pins 40-42', (102.8, 78.0, 104.5, 82.3)),
        ('B', 'BATTERY', 'J8', 'J8 PH-2 beside the cell top-right: BAT trunk ~14 mm', (115.5, 37.3, 123.5, 45.0)),
        ('B', 'BATTERY', 'NTC', 'R20 at the cell edge', (115.2, 95.0, 117.0, 96.3)),
        ('B', 'DEBUG', 'SWD', 'TC2030-NL behind the MCU', (64.0, 64.0, 72.0, 69.0)),
        ('B', 'DEBUG', 'TP', 'rail + console pads (2 x 6 @ 2.54)', (64.0, 48.0, 74.0, 62.0)),
        ('F', 'POWER', 'ESD', 'VBUS TVS + USB D+/D- ESD at J7 (new)', (88.0, 41.5, 92.5, 44.6)),
    ],
    keepouts=[
        ('F', 'low', 'under panel: parts <= 2.0 mm', R(62.95, 45.60, 133.85, 96.40)),
        ('F', 'zero', 'folded FPC: no parts', R(111.65, 48.05, 133.55, 93.95)),
        ('B', 'cell', 'cell: no parts', R(75.5, 39.5, 115.0, 102.5)),
    ],
)

COMMON_KO = [
    ('A', 'rf', 'BT830 antenna: no copper, any layer', R(*ANT)),
    ('F', 'zero', 'D-pad pivot', circ(*DPAD_PIVOT)),
] + [('A', 'hole', 'screw r3', circ(x, y, 3.0)) for x, y in SCREWS] + \
    [('B', 'post', 'key post', circ(x, y, 2.5)) for x, y in POSTS]

# ---------------------------------------------------------------- critical routes (polylines through the real gaps)
ROUTES = {   # endpoints at the package pin sides (U1 LQFP64 body 10x10 in its block; U2 QFN48 7x7), not block edges
 'T': [
  ('bus', 'Display bus x13 (J3 -> U2): L1 under the panel, 2 vias/net', [(106.2, 74.25), (107.5, 74.25), (107.5, 48.5), (116.0, 48.5), (119.0, 43.6)], 45),
  ('fast', 'PPU SPI x5 (U1 -> U2)', [(142.0, 46.0), (128.5, 44.0), (124.5, 41.0)], 20),
  ('fast', 'SPI3 x4 (U1 -> NOR on F)', [(142.0, 42.0), (128.6, 39.0), (124.5, 38.2)], 20),
  ('fast', 'USB FS D+/D- (J7 -> U1 PA11/12), 90R pair', [(95.25, 38.5), (95.25, 45.0), (137.5, 45.0), (142.0, 47.0)], 55),
  ('slow', 'BT UART x4 + BT_EN (U1 -> U12)', [(142.0, 50.0), (130.5, 46.6), (62.0, 46.6), (36.0, 53.0)], 115),
  ('slow', 'I2C1 (U1 -> U7 -> U11 -> U8)', [(142.0, 47.0), (119.4, 42.5), (113.0, 46.0), (60.0, 46.0), (52.0, 52.0), (45.0, 75.0), (40.0, 84.5)], 135),
  ('pwr', 'BAT 1.5 A (J8 -> U9): via array straight up', [(108.9, 41.0), (107.5, 39.5)], 5),
  ('pwr', 'SYS 1 A (Q1 -> amp U6)', [(98.55, 43.4), (112.0, 44.8), (133.0, 44.8), (148.5, 50.5)], 60),
  ('pwr', '+5V_LCD 80 mA (U5 -> backlight)', [(69.8, 41.0), (69.8, 46.2), (104.0, 46.2), (108.0, 50.0)], 50),
  ('slow', 'Right-grip keys + I2S (local)', [(150.0, 53.5), (150.0, 52.0), (156.9, 61.9)], 25),
 ],
 'F': [
  ('bus', 'Display bus x13 (J3 -> U2): L1, one layer, no vias', [(105.0, 63.25), (101.0, 66.0), (99.0, 72.0)], 12),
  ('fast', 'PPU SPI x5 (U1 -> U2)', [(85.0, 62.0), (85.0, 66.0), (95.25, 71.9)], 20),
  ('fast', 'SPI3 x4 (U1 -> NOR)', [(79.0, 60.0), (74.0, 66.0), (69.5, 73.0)], 20),
  ('fast', 'USB FS D+/D- (J7 -> U1), 90R pair', [(95.25, 38.5), (95.25, 45.0), (88.0, 47.0), (88.0, 52.0)], 20),
  ('slow', 'BT UART x4 + BT_EN (U1 -> U12)', [(79.0, 57.0), (60.0, 55.0), (36.0, 55.0)], 45),
  ('slow', 'I2C1 (U1 -> U7, U11, U8)', [(78.0, 67.0), (78.0, 70.0), (74.5, 68.0), (64.0, 60.0), (52.0, 52.0), (45.0, 75.0), (40.0, 84.5)], 80),
  ('pwr', 'BAT 1.5 A (J8 -> U9)', [(119.5, 41.0), (110.0, 41.0), (107.5, 39.5)], 15),
  ('pwr', 'SYS 1 A (Q1 -> amp U6)', [(98.55, 43.4), (112.0, 44.8), (133.0, 44.8), (148.5, 50.5)], 60),
  ('pwr', '+5V_LCD 80 mA (U5 -> backlight)', [(69.8, 41.0), (69.8, 46.2), (98.0, 46.2), (100.5, 50.5)], 45),
  ('slow', 'Right-grip keys + I2S (U1 -> right grip)', [(90.0, 52.0), (104.0, 52.0), (104.0, 47.5), (136.0, 47.5), (148.5, 52.0)], 70),
 ],
}

def length(pts): return sum(math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1))

# ---------------------------------------------------------------- checks
def kept_parts(v):
    moved = set(MOVED)
    if v['key'] == 'F': moved -= {'C51', 'R31'}            # 0.8 mm parts may stay under a floating panel
    out = []
    for p in BOARD['parts']:
        if p['ref'] in moved: continue
        side = 'F' if p['side'] == 'top' else 'B'
        xs = [q[0] for q in p['courtyard']]; ys = [q[1] for q in p['courtyard']]
        out.append((p['ref'], side, p['tht'], R(min(xs), min(ys), max(xs), max(ys)), p['height']))
    return out

def board_poly(v):
    b = EDGE
    for h in HOLES: b = b.difference(h)
    if v['opening']: b = b.difference(R(*v['opening']))
    return b

def check(v):
    issues = []; bp = board_poly(v); inner = bp.buffer(-0.5)
    kos = v['keepouts'] + COMMON_KO
    kept = kept_parts(v)
    for side, cat, name, members, bb in v['blocks']:
        g = R(*bb)
        if not inner.contains(g): issues.append(f'{name}: outside board-0.5 by {g.difference(inner).area:.2f} mm2')
        for ks, kind, lab, kg in kos:
            if name == 'J3' and kind == 'zero' and 'sweep' in lab: continue
            if (ks in (side, 'A')) and kind != 'low' and g.intersection(kg).area > 0.01:
                issues.append(f'{name} ({side}) hits keep-out "{lab}" {g.intersection(kg).area:.2f} mm2')
        for ref, ps, tht, pg, h in kept:
            if (ps == side or tht) and g.intersection(pg).area > 0.01:
                issues.append(f'{name} ({side}) overlaps kept {ref} {g.intersection(pg).area:.2f} mm2')
        for s2, c2, n2, m2, b2 in v['blocks']:
            if n2 > name and s2 == side and g.intersection(R(*b2)).area > 0.01:
                issues.append(f'{name} overlaps {n2}')
    # kept parts that now collide with this variant's keep-outs (must move too)
    for ref, ps, tht, pg, h in kept:
        for ks, kind, lab, kg in v['keepouts']:
            if (ks in (ps, 'A') or tht) and pg.intersection(kg).area > 0.01:
                if kind == 'low' and h <= 2.0: continue
                issues.append(f'kept {ref} ({ps}, h {h}) inside "{lab}"')
    return issues

def rf_distances(v):
    a = R(*ANT)
    d = {'panel frame': a.distance(R(*PANEL)), 'cell': a.distance(R(*v['cell'])),
         'nearest shell screw': min(a.distance(Point(*s)) for s in SCREWS),
         'speaker (magnet/frame)': a.distance(circ(*SPEAKER)), 'LRA': a.distance(circ(*LRA))}
    for side, cat, name, m, bb in v['blocks']:
        if name in ('MCU', 'FPGA'): d[name] = a.distance(R(*bb))
    d['MT3608 U5 (switch node)'] = a.distance(Point(69.8, 39.3)); d['TPS63021 U10/L2'] = a.distance(Point(82.0, 39.0))
    return {k: round(x, 1) for k, x in d.items()}

def area_free(v, side):
    """free board area on a side after keep-outs and kept parts (for the zone table)"""
    bp = board_poly(v)
    used = [kg for ks, kind, lab, kg in v['keepouts'] + COMMON_KO if ks in (side, 'A') and kind != 'low']
    used += [pg for ref, ps, tht, pg, h in kept_parts(v) if ps == side or tht]
    return bp.difference(unary_union(used)).area

if __name__ == '__main__':
    plan = {}
    for v in (T, F):
        iss = check(v); rf = rf_distances(v)
        rts = [(k, lab, round(length(p), 1), tgt) for k, lab, p, tgt in ROUTES[v['key']]]
        print(f"=== {v['key']}: {len(iss)} issues"); [print('  ', i) for i in iss]
        print('  RF distances from the antenna keep-out (mm):', rf)
        for r in rts: print('   route', r)
        print('  board area', round(board_poly(v).area), ' free F', round(area_free(v, 'F')), ' free B', round(area_free(v, 'B')))
        plan[v['key']] = dict(title=v['title'], cell=v['cell'], bay=v['bay'], opening=v['opening'],
                              zones=[dict(side=s, cat=c, name=n, box=b) for s, c, n, b in v['zones']],
                              blocks=[dict(side=s, cat=c, name=n, members=m, box=b) for s, c, n, m, b in v['blocks']],
                              keepouts=[dict(side=s, kind=k, label=l, bounds=[round(x, 2) for x in g.bounds]) for s, k, l, g in v['keepouts'] + COMMON_KO],
                              routes=[dict(kind=k, label=l, est_mm=e, target_mm=t) for k, l, e, t in rts],
                              rf_mm=rf, issues=iss)
    json.dump(plan, open(os.path.join(HERE, 'ee_plan.json'), 'w'), indent=1)
