#!/usr/bin/env python3
"""place.py — deterministic component placement for gb3.

Re-runnable floorplan for the gb3 handheld PCB. Reads footprint/pad data from the board once
(up front — pcbnew's Pads() iterator corrupts after edits), then places everything from plain data:

  * Front (F.Cu): the button switches, at their Dwgs.User outline positions.
  * Back  (B.Cu): every IC placed at its subcircuit's target point (SUBC_POS), each IC's bypass
                  caps ringed around the pins they serve (2.6mm standoff), remaining passives
                  row-packed next to it. A battery keepout (BAT_KEEPOUT) is reserved.
  * All placements keep a >=GAP clearance (collision-aware); nothing overlaps.

Floorplan intent: SoC centred (right of the battery), the whole power chain along the top
(fuel-gauge -> CN1 -> charger -> USB-C -> regulators), logic on the SoC side to minimise nets
crossing the battery, audio along the bottom, connectors at their board edges.

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 place.py
Then re-render with ../../../cad/render_pcb.py.
"""
import pcbnew, csv, math, os, sys

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
BOM  = os.path.join(os.path.dirname(__file__), "gb3-bom.csv")
GAP  = pcbnew.FromMM(0.6)           # min clearance between any two same-side parts (breathing room)
YMAX = pcbnew.FromMM(116)           # board bottom (parts stay above this; margin keeps courtyards on-board)
STANDOFF = pcbnew.FromMM(2.6)       # decoupling-ring gap outside a chip's pads
# battery + speaker + LRA are consolidated into ONE right-side cutout region (battery far right,
# speaker + LRA just left of it). Speaker back-fires (out a back-cover grille).
BAT_OUTLINE = (141.2, 54.3, 179.7, 116.3)  # PKCELL LP-803860 cell 38.5 (W) x 62 (L); right of the display cable's rigid body
BAT_BUFFER  = (139.2, 52.3, 181.7, 117.8)  # +2mm swell/handling buffer; bottom edge flush with the board's bottom edge
BAT_KEEPOUT = BAT_BUFFER             # battery sits in a THROUGH-cutout -> nothing (incl. cables) may share its XY
# transducers clear of the cables: speaker in the bottom-right corner (right of battery), LRA above the battery
SPEAKER = (197.3, 102.8, 13.5)      # Ø27 CES-2704-088L050 — bottom-right corner: buffer flush to the board's bottom+right
                                    # edges (X 182.3..212.3, Y 87.8..117.8), clearing the battery buffer (right edge 181.7) by 0.6
LRA     = (133.4, 77.2, 4)          # Ø8 VG0832022D coin — in the notch left of the battery (buffer X128.4..138.4,
                                    # Y72.2..82.2): 0.8 clear of the battery buffer (X139.2), right of the display
                                    # flex's narrow section (X<=103.9 at this Y), 1.7 above where that flex widens (Y83.9)
# direct-solder pads placed next to their transducer outlines so the flying leads stay short (not 40mm away)
SOLDER_PADS = {'J3': (187.9, 82.7), 'J2': (124.2, 57.7)}   # both sit on copper that survives the cutouts (draw_outline.py):
                                                           # J3 in the right block above the speaker opening (22mm of lead),
                                                           # J2 in the column left of the X127.5 cut wall (22mm to the LRA)
SOLDER_PAD_ROT = {'J2': 90}          # J2's pad pair runs vertically up the narrow column; J3 stays horizontal
SPEAKER_BUF = 1.5                    # mm mounting/handling buffer around the speaker (drawn + keepout)
LRA_BUF     = 1.0                    # mm buffer around the LRA
# U18 (BT830) is an RF module: integrated antenna at its +X end. Pin it to the top-LEFT corner at
# orientation 90 so the antenna radiates UP off the top edge (far from the battery on the right), and
# clear all-copper ground from the antenna out to the edge — the footprint only keeps F/B pour out, so
# the inner GND/PWR planes would otherwise fill under the antenna and detune it.
RF_U18 = (29, 46, 90)               # cx, cy, orientation deg (back side); antenna ends ~1.2 mm from top edge
ANT_LOCAL = (7.08, -6.30, 12.85, 0.0)  # U18 antenna keepout rect in the module's LOCAL frame (from the footprint), mm

# Front buttons: switch ref -> centre (mm), on the Dwgs.User outline circles.
FRONT_BTN = {'SW2':(28.5,74.9),'SW3':(44.5,74.9),'SW4':(36.5,66.9),'SW5':(36.5,82.9),
             'SW6':(190.2,74.9),'SW7':(206.2,74.9),'SW8':(198.2,66.9),'SW9':(198.2,82.9),
             'SW10':(36.5,50),'SW11':(198.2,50)}   # Select / Start — top of the grips (below the bumpers)
EDGE_BTN  = {'SW15':(42,38),'SW16':(193,38),'SW12':(209,46),'SW13':(209,54),'SW14':(209,62)}  # in from edges/corners (pads on-board)
# Connectors pinned to the board edges they exit. U4 (microSD) rotated so its slot faces the edge.
# FRONT_CONN sit on the top face; all other connectors (and everything else) live on the back.
# P1/FPC1 placed from the ATM0500D27 mechanical drawing. Screen outline is X[57..177.7], Y[37..112.8].
# Both flex tails exit the screen's BOTTOM edge (Y=112.8) and fold up behind the screen (tail 53.2mm).
#   Connector Y = 112.8 - (53.2 - ~2 bend - ~1 so the pins land on the contacts) ~= 64  (i.e. <53.2 up).
#   P1  (display 40P): 26.4mm right of the screen's LEFT edge  -> X = 57 + 26.4  = 83.4  (front, folds in the gap)
#   FPC1(touch  6P):   55.8mm from the screen's RIGHT edge     -> X = 177.7 - 55.8 = 121.9 (back, wraps the bottom edge)
# Hand-tuned in the GUI, then captured here (board edges: X[21.95..212.75] Y[31.95..117.85]):
#   USBC1  — pushed up so the shell overhangs the TOP edge (silk to Y30.6) and meets the enclosure wall.
#   U4     — microSD moved to the TOP edge at rot 0 (slot faces up): the card now inserts/ejects without
#            fouling the display/touch FPCs it used to sit on top of.
#   Audio1 — left grip, FRONT, rot 180 so the barrel overhangs the BOTTOM edge (silk to Y119.3).
#   CN1    — battery JST moved FRONT, right of the screen (screen right edge 177.7). It is a tall part, so
#            on the front beside the screen it hides under the ~6.25mm screen-front profile instead of
#            adding to the console's back depth.
CONN = {'USBC1':(118,35.72),'U4':(93.2,41.76),'P1':(93.7,66.8),'FPC1':(118.5,68.6),
        'Audio1':(46.6,110.38),'CN1':(186.7,45.6)}
FRONT_CONN = {'P1', 'Audio1', 'CN1'}   # display FPC + headphone jack + battery JST on the top face
CONN_ROT = {'Audio1': 180}   # Audio1 barrel/opening faces the bottom edge (barrel is -Y at rot 0); U4 slot faces up at rot 0
# Each subcircuit's hub-IC target centre (mm). Battery is RIGHT half, so SoC + most logic go LEFT
# (next to the left display connector for a short display bus); power chain across the top.
# cutout region takes the right-centre, so the SoC + almost everything sit in the LEFT block; the amp
# and haptics hug the left edge of the cutout (short leads to the speaker/LRA inside it).
SUBC_POS = {'soc':(68,74),'usb-c':(112,46),'charger':(68,40),'core-buck':(45,40),'buck-boost':(30,42),
            'fuel-gauge':(135,37),'power-monitor':(160,37),'power-ctrl':(95,95),'backlight':(28,56),
            'nor':(45,88),'clock':(40,55),'i2c':(80,60),'imu':(45,100),
            'expander':(95,84),'bluetooth':(34,54),   # BT caps cluster just below the corner-pinned U18
            'haptics':(123.9,47),  # DRV2605L + caps in the column left of the X127.5 cutout wall (see draw_outline.py).
                                   # Must stay left of 127.5: everything right of it below Y52.3 is cut away.
            'dac':(35,105),'headphone-amp':(60,108),'amp':(102,62),
            'microsd':(93,52),   # follows U4 to the top edge: ESD arrays + pullups tuck just below the socket
            'power-led':(112,108),'display':(80,62),'touch':(96,60)}
# Placement order: SoC first, then power, logic, audio.  Battery is RIGHT half, so the SoC and nearly
# all of its peripherals sit LEFT/centre; only the ABXY grip (front) is inherently on the right.
ORDER = ['soc','usb-c','charger','core-buck','buck-boost','fuel-gauge','power-monitor','power-ctrl',
         'backlight','nor','clock','imu','expander','bluetooth','haptics','dac','headphone-amp','amp',
         'microsd','i2c','power-led','display','touch']

MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(x), int(y))
isg = lambda n: 'GND' in (n or '').upper() or (n or '').upper() in ('VSS', 'AGND', 'DGND')


def main():
    B = pcbnew.LoadBoard(PATH)
    BOARDPOLY = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(BOARDPOLY, True)  # real (rounded) board outline
    refs = [fp.GetReference() for fp in B.GetFootprints()]
    fps = {r: B.FindFootprintByReference(r) for r in refs}
    for z in list(B.Zones()):                          # drop the prior RF keepout NOW (B.Zones() is unsafe after mutations)
        if z.GetZoneName() == 'U18_ANT_KEEPOUT': B.Remove(z)
    # --- read pad data ONCE, before any mutation (Pads() corrupts after edits) ---
    info = {}
    for r in refs:
        fp = fps[r]; p0 = fp.GetPosition(); pads = []; xs = []; ys = []
        for pad in fp.Pads():
            pp = pad.GetPosition(); s = pad.GetSize()
            pads.append((pad.GetNetname(), pp.x - p0.x, pp.y - p0.y))
            xs += [pp.x - s.x // 2, pp.x + s.x // 2]; ys += [pp.y - s.y // 2, pp.y + s.y // 2]
        if xs: w, h = max(xs) - min(xs), max(ys) - min(ys)
        else:  w, h = MM(2), MM(2)     # no-pad footprints (e.g. 3D-only mech refs); avoid GetBoundingBox (wxApp crash)
        info[r] = {'pads': pads, 'w': w, 'h': h, 'n': len(pads)}
    sub = {row['refdes']: row['subcircuit'] for row in csv.DictReader(open(BOM))}

    occ = {True: [], False: []}; final = {}
    def size(r, o):
        w, h = info[r]['w'], info[r]['h']; return (h, w) if o % 180 == 90 else (w, h)
    def collide(l, t, r, b, f):
        return any((min(r, R) - max(l, L)) > -GAP and (min(b, Bt) - max(t, T)) > -GAP
                   for (L, T, R, Bt) in occ[f])
    def place(ref, cx, cy, o=0, f=True):
        fp = fps[ref]
        if f != fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        fp.SetPosition(V(cx, cy)); fp.SetOrientationDegrees(o)
        w, h = size(ref, o); occ[f].append((cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2))
        final[ref] = (int(cx), int(cy), o % 180 == 90, f)
    XMIN, XMAX, YMIN = MM(24), MM(211), MM(32)     # grid-search bounds; the real check is the polygon below
    def onboard(l, t, r, b):                       # all four pad-box corners inside the real (rounded) board
        return all(BOARDPOLY.Contains(pcbnew.VECTOR2I(int(x), int(y))) for x, y in [(l, t), (r, t), (l, b), (r, b)])
    def place_free(ref, cx, cy, o=0, f=True):     # spiral outward from the target to the nearest free, on-board spot
        w, h = size(ref, o)
        for step in range(240):
            R = step * MM(0.5)
            for k in range(max(1, int(2 * math.pi * R / MM(1.3)))):
                ang = 2 * math.pi * k / max(1, int(2 * math.pi * R / MM(1.3)))
                x = int(cx + R * math.cos(ang)); y = int(cy + R * math.sin(ang))
                l, t, r, b = x - w // 2, y - h // 2, x + w // 2, y + h // 2
                if onboard(l, t, r, b) and not collide(l, t, r, b, f):
                    place(ref, x, y, o, f); return
        place(ref, cx, cy, o, f)
    def rot(x, y, d):
        d %= 360; return (x, y) if d == 0 else (-y, x) if d == 90 else (-x, -y) if d == 180 else (y, -x)
    def ring(hub, hcx, hcy, caps):                # ring a hub's bypass caps around the pins they serve
        P = info[hub]['pads']; pins = {}
        for net, ox, oy in P: pins.setdefault(net, []).append((-ox, oy))
        hx = max((abs(ox) for _, ox, oy in P), default=MM(0.8)); hy = max((abs(oy) for _, ox, oy in P), default=MM(0.8))
        mp = []
        for r in caps:
            rl = [n for n, ox, oy in info[r]['pads'] if not isg(n)]; rail = rl[0] if rl else None
            if rail in pins:
                pl = pins[rail]; ang = math.atan2(sum(b for _, b in pl) / len(pl), sum(c for c, _ in pl) / len(pl))
                mp.append((ang, r))
        mp.sort(); n = len(mp)
        if not n: return
        hw = hx + STANDOFF; hh = hy + STANDOFF; W, H = 2 * hw, 2 * hh; Per = 2 * (W + H)
        def pm(fr):
            d = fr * Per
            if d < W: return (-hw + d, -hh, 90)
            d -= W
            if d < H: return (hw, -hh + d, 0)
            d -= H
            if d < W: return (hw - d, hh, 90)
            d -= W
            return (-hw, hh - d, 0)
        for i, (ang, r) in enumerate(mp):
            rx, ry, o = pm((i + 0.5) / n); cx, cy = hcx + int(rx), hcy + int(ry)
            pd = {('gnd' if isg(nn) else 'rail'): (-ox, oy) for nn, ox, oy in info[r]['pads']}
            best = o
            if 'rail' in pd:                      # rail pad toward the chip, GND pad outward
                f0 = rot(*pd['rail'], o); f1 = rot(*pd['rail'], o + 180)
                best = o if (cx + f0[0] - hcx) ** 2 + (cy + f0[1] - hcy) ** 2 <= (cx + f1[0] - hcx) ** 2 + (cy + f1[1] - hcy) ** 2 else o + 180
            w, h = size(r, best); dx, dy = cx - hcx, cy - hcy; L = math.hypot(dx, dy) or 1; ux, uy = dx / L, dy / L
            done = False
            for k in range(120):                  # nudge radially out until clear of neighbours
                tx, ty = int(cx + ux * k * MM(0.3)), int(cy + uy * k * MM(0.3))
                if not onboard(tx - w // 2, ty - h // 2, tx + w // 2, ty + h // 2): break   # heading off-board
                if not collide(tx - w // 2, ty - h // 2, tx + w // 2, ty + h // 2, True):
                    place(r, tx, ty, best, True); done = True; break
            if not done:                          # ring spot ran off the edge -> bounded fallback
                place_free(r, cx, cy, best, True)

    # ONE pre-mutation pass: remove ALL old battery/speaker/LRA/flex outlines (GetDrawings corrupts after B.Add)
    for d in list(B.GetDrawings()):
        cls = d.GetClass()
        if cls == 'PCB_SHAPE' and d.GetLayer() == pcbnew.Dwgs_User:
            sh = d.GetShape()
            if sh == pcbnew.SHAPE_T_RECT:
                w = abs(TO(d.GetEnd().x) - TO(d.GetStart().x)); h = abs(TO(d.GetEnd().y) - TO(d.GetStart().y))
                if 34 < w < 44 and 58 < h < 68: B.Remove(d)      # battery cell + buffer outlines
            elif sh == pcbnew.SHAPE_T_CIRCLE:
                c = d.GetStart(); rr = math.hypot(TO(d.GetEnd().x) - TO(c.x), TO(d.GetEnd().y) - TO(c.y))
                if TO(c.x) < 215 and (3 < rr < 6 or 12 < rr < 16): B.Remove(d)   # LRA/speaker body + buffer circles
            elif sh == pcbnew.SHAPE_T_SEGMENT:
                B.Remove(d)                                      # FPC flex-tail outlines (only they use SEG here)
        elif cls == 'PCB_TEXT' and any(d.GetText().strip().startswith(p) for p in ('BATTERY', 'LRA', 'SPEAKER', 'FPC-')):
            B.Remove(d)
    for (x1, y1, x2, y2), wid in ((BAT_OUTLINE, 0.25), (BAT_BUFFER, 0.15)):   # cell body (bold) + swell buffer (thin)
        r = pcbnew.PCB_SHAPE(B); r.SetShape(pcbnew.SHAPE_T_RECT)
        r.SetStart(V(MM(x1), MM(y1))); r.SetEnd(V(MM(x2), MM(y2)))
        r.SetLayer(pcbnew.Dwgs_User); r.SetWidth(MM(wid)); B.Add(r)
    t = pcbnew.PCB_TEXT(B); t.SetText("BATTERY PKCELL LP-803860  38.5x62 (+2mm buffer)"); t.SetLayer(pcbnew.Dwgs_User)
    t.SetPosition(V(MM(BAT_BUFFER[0]), MM(BAT_BUFFER[1] - 2))); t.SetTextSize(V(MM(1.8), MM(1.8)))
    t.SetTextThickness(MM(0.22)); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)
    occ[True].append((MM(BAT_KEEPOUT[0]), MM(BAT_KEEPOUT[1]), MM(BAT_KEEPOUT[2]), MM(BAT_KEEPOUT[3])))
    # speaker + LRA reference outlines (Dwgs.User) and their component keepouts (old ones removed above)
    # each transducer: bold body circle + thin buffer circle (like the battery cell + swell buffer)
    for cx, cy, rad, buf, lbl in ((SPEAKER[0], SPEAKER[1], SPEAKER[2], SPEAKER_BUF, "SPEAKER Ø27 (CES-2704-088L050) +1.5mm buffer"),
                                  (LRA[0], LRA[1], LRA[2], LRA_BUF, "LRA Ø8 (VG0832022D) +1mm buffer")):
        for r_, wid in ((rad, 0.25), (rad + buf, 0.15)):        # body (bold), buffer (thin)
            c = pcbnew.PCB_SHAPE(B); c.SetShape(pcbnew.SHAPE_T_CIRCLE); c.SetLayer(pcbnew.Dwgs_User); c.SetWidth(MM(wid))
            c.SetStart(V(MM(cx), MM(cy))); c.SetEnd(V(MM(cx + r_), MM(cy))); B.Add(c)
        t = pcbnew.PCB_TEXT(B); t.SetText(lbl); t.SetLayer(pcbnew.Dwgs_User); t.SetPosition(V(MM(cx - rad - buf), MM(cy - rad - buf - 1.5)))
        t.SetTextSize(V(MM(1.6), MM(1.6))); t.SetTextThickness(MM(0.2)); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)
    for cx, cy, rad, buf in ((*SPEAKER, SPEAKER_BUF), (*LRA, LRA_BUF)):           # keepout = body + buffer
        occ[True].append((MM(cx - rad - buf), MM(cy - rad - buf), MM(cx + rad + buf), MM(cy + rad + buf)))
    for r, (x, y) in FRONT_BTN.items(): place(r, MM(x), MM(y), 0, False)
    # SKRTLAE010 actuator faces +y (down) at 0deg (per 3D model). Point each plunger OUT of the board:
    # bumpers (top edge) actuate up = 180deg; vol/pwr (right edge) actuate right = 90deg.
    EDGE_ROT = {'SW12': 90, 'SW13': 90, 'SW14': 90, 'SW15': 180, 'SW16': 180}
    for r, (x, y) in EDGE_BTN.items():  place(r, MM(x), MM(y), EDGE_ROT.get(r, 0), False)
    for r, (x, y) in CONN.items():      place(r, MM(x), MM(y), CONN_ROT.get(r, 0), r not in FRONT_CONN)  # U4/Audio1 rotations in CONN_ROT
    place('U18', MM(RF_U18[0]), MM(RF_U18[1]), RF_U18[2], True)   # RF module: fixed top-left corner, antenna up off top edge
    for r, (x, y) in SOLDER_PADS.items(): place(r, MM(x), MM(y), SOLDER_PAD_ROT.get(r, 0), True)  # transducer solder pads
    fixed = set(CONN) | set(FRONT_BTN) | set(EDGE_BTN) | {'U18'} | set(SOLDER_PADS)

    # (FPC flex-tail outlines removed — the folded cable landings on Cmts.User (fold_cables.py) are the single
    #  flex representation now. place.py's pre-mutation pass above still clears any stale Dwgs.User SEG flex.)

    # antenna ground keepout: clear all-copper (incl. inner GND/PWR planes) from U18's antenna out past the top edge.
    # Transform the LOCAL antenna rect to board coords by RF_U18's placement (avoids reading footprint.Zones() post-mutation).
    def ant_xf(lx, ly):                                # module local -> board mm (back-side flip negates Y, then rotate)
        lx, ly = lx, -ly
        th = RF_U18[2] % 360
        if   th == 90:  lx, ly = ly, -lx
        elif th == 180: lx, ly = -lx, -ly
        elif th == 270: lx, ly = -ly, lx
        return RF_U18[0] + lx, RF_U18[1] + ly
    corners_l = [(ANT_LOCAL[0], ANT_LOCAL[1]), (ANT_LOCAL[2], ANT_LOCAL[1]), (ANT_LOCAL[2], ANT_LOCAL[3]), (ANT_LOCAL[0], ANT_LOCAL[3])]
    bxy = [ant_xf(x, y) for x, y in corners_l]
    ax0 = MM(min(p[0] for p in bxy)); ax1 = MM(max(p[0] for p in bxy))
    ay1 = MM(max(p[1] for p in bxy))                   # bottom of the antenna rect (top handled by the board edge below)
    m = MM(0.8)
    corners = [(ax0 - m, MM(30.5)), (ax1 + m, MM(30.5)), (ax1 + m, ay1 + m), (ax0 - m, ay1 + m)]  # up to past top edge
    ko = pcbnew.ZONE(B); ko.SetIsRuleArea(True); ko.SetZoneName('U18_ANT_KEEPOUT')
    ko.SetDoNotAllowZoneFills(True); ko.SetDoNotAllowVias(True); ko.SetDoNotAllowTracks(True)
    ls = pcbnew.LSET()
    for L in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu): ls.AddLayer(L)
    ko.SetLayerSet(ls)
    ch = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in corners: ch.Append(int(x), int(y))
    ch.SetClosed(True); ko.AddPolygon(ch)             # AddPolygon (stable); Outline() proxy is flaky
    B.Add(ko)
    for sc in ORDER:
        parts = [r for r in refs if sub.get(r) == sc and r not in final and r not in fixed]
        cand = [r for r in parts if r.startswith('U')]
        hub = max(cand, key=lambda r: info[r]['n']) if cand else None
        px, py = SUBC_POS.get(sc, (165, 70))
        if hub:
            place_free(hub, MM(px), MM(py)); hc = final[hub]
            ring(hub, hc[0], hc[1], [r for r in parts if r.startswith('C')])
        rest = [r for r in parts if r not in final]; rest.sort(key=lambda r: -info[r]['w'])
        for r in rest:                            # cluster remaining passives around the hub (spiral out)
            place_free(r, MM(px), MM(py + (9 if hub else 0)))

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())         # re-fill GND/PWR planes for the new positions

    # report: overlaps, off-edge, and nets crossing the battery
    def box(r):
        cx, cy, r9, f = final[r]; w, h = info[r]['w'], info[r]['h']
        if r9: w, h = h, w
        return cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2, f
    allr = [r for r in refs if r in final]; ov = 0
    for i in range(len(allr)):
        for j in range(i + 1, len(allr)):
            a = box(allr[i]); b = box(allr[j])
            if a[4] != b[4]: continue
            if (min(a[2], b[2]) - max(a[0], b[0])) > 0 and (min(a[3], b[3]) - max(a[1], b[1])) > 0: ov += 1
    # Off-board check against the REAL outline (BOARDPOLY), not a bounding rect: since draw_outline.py cut the
    # battery/speaker/LRA/cable-notch regions out, a rect test would happily pass a part floating in a cutout.
    # This tests pad boxes only, so Audio1/USBC1 (whose bodies deliberately overhang an edge) don't false-trip.
    edge = [r for r in allr if not (lambda c: onboard(c[0], c[1], c[2], c[3]))(box(r))]
    railkey = ('+3V3', '+5V', 'SYS', 'VBUS', 'VBAT', 'VDD', 'VCC', 'BAT', 'AVCC', 'HPVCC', 'VREG')
    netx = {}
    for r in allr:
        for net, ox, oy in info[r]['pads']:
            if net and not isg(net) and not net.startswith('unconnected') and not any(k in net.upper() for k in railkey):
                netx.setdefault(net, set()).add(final[r][0])
    cross = sorted(n for n, xs in netx.items() if min(xs) < MM(BAT_KEEPOUT[0]) and max(xs) > MM(BAT_KEEPOUT[2]))

    pcbnew.SaveBoard(PATH, B)
    print("overlaps:", ov, "| off-edge:", edge)
    print("cross-battery signal nets:", len(cross), "->", cross)
    sys.stdout.flush(); sys.stderr.flush()
    os._exit(0)          # skip Python/swig teardown, which segfaults on pcbnew objects (work is already saved)


if __name__ == "__main__":
    main()
