#!/usr/bin/env python3
"""draw_console.py — draw the ~195 x 90 mm console outline around the screen sketch, centered on the
User.2 screen rectangle (the screen is centered in the console on the real device). Rounded rect.

Drawn on Eco1.User (re-runnable: clears Eco1.User first).
Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 draw_console.py
"""
import pcbnew, os, sys, math

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(x)), int(round(y)))

CONSOLE_W, CONSOLE_H, R = 194.7, 89.8, 7.1      # matches the "OUR DEVICE" console exactly (size + corner radius), mm


def main():
    B = pcbnew.LoadBoard(PATH)
    dst = B.GetLayerID('Eco1.User')
    u2 = B.GetLayerID('User.2')

    # ONE pass over drawings BEFORE any mutation (GetDrawings corrupts after B.Add/Remove):
    # find the User.2 screen centre, snapshot the battery/speaker/LRA source geometry, and mark Eco1.User for clearing.
    cx = cy = None; bat_rects = []; trans_circ = []; to_clear = []
    for d in list(B.GetDrawings()):
        if d.GetLayer() == dst:
            to_clear.append(d); continue
        if d.GetClass() != 'PCB_SHAPE':
            continue
        sh = d.GetShape()
        if sh == pcbnew.SHAPE_T_RECT:
            ax0, ay0, ax1, ay1 = TO(d.GetStart().x), TO(d.GetStart().y), TO(d.GetEnd().x), TO(d.GetEnd().y)
            if d.GetLayer() == u2:                                     # the screen rect -> centre
                cx = (ax0 + ax1) / 2; cy = (ay0 + ay1) / 2
            elif d.GetLayer() == pcbnew.Dwgs_User and 36 < abs(ax1 - ax0) < 44 and min(ay0, ay1) < 125:
                bat_rects.append((ax0, ay0, ax1, ay1, d.GetWidth()))   # battery cell/buffer
        elif sh == pcbnew.SHAPE_T_CIRCLE and d.GetLayer() == pcbnew.Dwgs_User:
            c = d.GetStart(); ex, ey = TO(d.GetEnd().x), TO(d.GetEnd().y); rr = math.hypot(ex - TO(c.x), ey - TO(c.y))
            if TO(c.x) < 215 and TO(c.y) < 125 and (3.9 < rr < 5.1 or 13 < rr < 15.5):   # our board only (Switch is at X>250)
                trans_circ.append((TO(c.x), TO(c.y), ex, ey, d.GetWidth()))   # speaker/LRA body+buffer
    if cx is None:
        sys.exit("no User.2 screen rect found")
    for d in to_clear:
        B.Remove(d)

    x0, y0 = cx - CONSOLE_W / 2, cy - CONSOLE_H / 2
    x1, y1 = cx + CONSOLE_W / 2, cy + CONSOLE_H / 2
    # rounded-rect perimeter points (4 corner arcs; straight edges fall out between them)
    n = 10; pts = []
    for ccx, ccy, a0, a1 in ((x0 + R, y0 + R, 180, 270), (x1 - R, y0 + R, 270, 360),
                             (x1 - R, y1 - R, 0, 90), (x0 + R, y1 - R, 90, 180)):
        for i in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * i / n)
            pts.append((ccx + R * math.cos(a), ccy + R * math.sin(a)))
    for i in range(len(pts)):
        a = pts[i]; b = pts[(i + 1) % len(pts)]
        s = pcbnew.PCB_SHAPE(B); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetLayer(dst); s.SetWidth(MM(0.3))
        s.SetStart(V(MM(a[0]), MM(a[1]))); s.SetEnd(V(MM(b[0]), MM(b[1]))); B.Add(s)

    t = pcbnew.PCB_TEXT(B); t.SetText("CONSOLE outline ~195 x 90 (screen centred)"); t.SetLayer(dst)
    t.SetPosition(V(MM(x0 + 3), MM(y0 - 3))); t.SetTextSize(V(MM(2), MM(2))); t.SetTextThickness(MM(0.25))
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)

    # (battery/speaker/LRA are drawn by place.py in the board frame — no sketch copy needed now)

    pcbnew.SaveBoard(PATH, B)
    print("console outline: X %.1f..%.1f  Y %.1f..%.1f  (centre %.1f,%.1f)" % (x0, x1, y0, y1, cx, cy))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
