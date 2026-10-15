#!/usr/bin/env python3
"""verify.py — the pre-fab footprint checks that can be done from data (no hardware), on the saved board.

  1. every IC / transistor / crystal footprint: pins numbered counter-clockwise seen from the top (a CW footprint is
     mirrored - the classic bottom-view mistake), and pin 1 at the corner the easyeda footprint name declares
     (-TL / -TR / -BL / -BR)
  2. J3 / J4 against the Adafruit 2090: every socket pad on its module header pin (draw_mech's module placement,
     from Adafruit's .brd), and each pad's net against the net on that pin in Adafruit's own schematic

Run:  <kicad python> fab/verify.py      (exit 1 on any failure)
"""
import os, re, sys, math
BRD = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."); os.chdir(BRD); sys.path.insert(0, BRD)
import pcbnew, draw_mech as M
TO = pcbnew.ToMM; B = pcbnew.LoadBoard("gb2b.kicad_pcb"); bad = 0
libs = {}
for l in open("fp-lib-table").read().split("(lib ")[1:]:
    libs[re.search(r'\(name "?([^")]+)"?\)', l).group(1)] = re.search(r'\(uri "?([^")]+)"?\)', l).group(1).replace("${KIPRJMOD}", os.getcwd())
print("1. footprint pin numbering (library copy, top view)")
for fp in sorted(B.GetFootprints(), key=lambda f: f.GetReference()):
    r = fp.GetReference()
    if not re.match(r"(U|Q|Y)\d", r): continue
    lid = fp.GetFPID(); name = lid.GetLibItemName().wx_str()
    L = pcbnew.FootprintLoad(libs[lid.GetLibNickname().wx_str()], name); o = L.GetPosition()
    pads = sorted({int(p.GetNumber()): (TO(p.GetPosition().x - o.x), -TO(p.GetPosition().y - o.y)) for p in L.Pads() if p.GetNumber().isdigit()}.items())
    cx = sum(x for _, (x, y) in pads) / len(pads); cy = sum(y for _, (x, y) in pads) / len(pads)
    ring = [(k, xy) for k, xy in pads if math.hypot(xy[0] - cx, xy[1] - cy) > 0.3]
    ang = [math.atan2(y - cy, x - cx) for _, (x, y) in ring]
    turn = sum((b - a + math.pi) % (2 * math.pi) - math.pi for a, b in zip(ang, ang[1:]))
    x1, y1 = ring[0][1]; corner = ("T" if y1 > cy else "B") + ("L" if x1 < cx else "R")
    m = re.search(r"-(TL|TR|BL|BR)\b", name); want = m.group(1) if m else None
    ok = turn > 0 and (want is None or want == corner)
    bad += not ok
    print(f"  {'ok ' if ok else 'BAD'} {r:4s} {name[:40]:40s} {len(ring):2d} pins, {'CCW' if turn > 0 else 'CW (mirrored?)'}, pin 1 {corner}" + (f" (name says {want})" if want else ""))
print("2. J3 / J4 vs the Adafruit 2090")
S = M.stretch(); _, _, (mcx, mcy), sx = M.layout(S); gx, gy = M.GBA_ORIGIN
sch = open("../../../../cad/docs/adafruit-2090-cap-touch-revD.sch").read(); pinnet = {}
for m in re.finditer(r'<net name="([^"]+)"[^>]*>(.*?)</net>', sch, re.S):
    for p in re.finditer(r'<pinref part="(JP[12])" gate="G\$1" pin="(\d+)"/>', m.group(2)): pinnet[(p.group(1), int(p.group(2)))] = m.group(1)
ADAFRUIT = {"GND": "GND", "+5V_LCD": ("+5V", "LCD_RD_5V"), "LCD_CS": "LCD_CS_5V", "LCD_DC": "LCD_RS_5V",
            "LCD_WR": "LCD_WR_5V", "LCD_RST": "LCD_RST_5V", "LCD_LITE": "LCD_LITE",
            **{f"LCD_D{k}": f"LCD_DATA{k}_5V" for k in range(8)}}           # our net -> Adafruit's net(s) on that pin
EXPECT = lambda ours, theirs: theirs in (ADAFRUIT.get(ours) if isinstance(ADAFRUIT.get(ours), tuple) else (ADAFRUIT.get(ours),))
for hdr, ref, xs in (("JP1", "J3", -38.1), ("JP2", "J4", 38.1)):
    worst, nbad = 0.0, 0
    for p in B.FindFootprintByReference(ref).Pads():
        n = int(p.GetNumber()); q = p.GetPosition()
        worst = max(worst, math.hypot(TO(q.x) - (gx + mcx + sx * xs), TO(q.y) - (gy + mcy + 24.13 - 2.54 * (n - 1))))
        ours = p.GetNetname() if not p.GetNetname().startswith("unconnected") else "-"
        if ours != "-" and not EXPECT(ours, pinnet.get((hdr, n), "?")):
            nbad += 1; print(f"  BAD {ref}.{n}: board {ours} vs 2090 {pinnet.get((hdr, n))}")
    ok = worst < 0.01 and not nbad; bad += not ok
    print(f"  {'ok ' if ok else 'BAD'} {ref} on {hdr}: worst pad-to-pin offset {worst:.3f} mm, nets {'match' if not nbad else 'MISMATCH'}")
sys.exit(1 if bad else 0)
