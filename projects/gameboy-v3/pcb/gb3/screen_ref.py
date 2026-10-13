#!/usr/bin/env python3
"""screen_ref.py — recreate the ATM0500D27 5" TFT mechanical drawing to scale on Dwgs.User.

Drawn in empty space BELOW the board (LCM top-left at X0,Y0) so it can be checked against the
datasheet before we replace the (previously wrong) on-board screen outline. Corrects the earlier
mistake: 120.7 x 75.8 is the OUTER module (LCM), the inner areas are TP window 108.8 x 66.2 and
LCD active 108.0 x 64.8.

Re-runnable: clears its own prior drawing (all Dwgs.User items below Y=119) then redraws.
Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 screen_ref.py
"""
import pcbnew, os, sys

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(x), int(y))

X0, Y0 = 57.0, 132.0        # LCM top-left corner, placed below the board (board ends at Y~117.8)


def main():
    B = pcbnew.LoadBoard(PATH)
    # clear any prior screen-ref (everything we drew lives on Dwgs.User below Y=119)
    for d in list(B.GetDrawings()):
        if d.GetLayer() != pcbnew.Dwgs_User:
            continue
        try:
            y = TO(d.GetStart().y) if d.GetClass() == 'PCB_SHAPE' else TO(d.GetPosition().y)
        except Exception:
            continue
        if y > 119:
            B.Remove(d)

    def rect(x0, y0, x1, y1, w):
        r = pcbnew.PCB_SHAPE(B); r.SetShape(pcbnew.SHAPE_T_RECT); r.SetLayer(pcbnew.Dwgs_User); r.SetWidth(MM(w))
        r.SetStart(V(MM(X0 + x0), MM(Y0 + y0))); r.SetEnd(V(MM(X0 + x1), MM(Y0 + y1))); B.Add(r)

    def label(x, y, s, sz=1.3):
        t = pcbnew.PCB_TEXT(B); t.SetText(s); t.SetLayer(pcbnew.Dwgs_User)
        t.SetPosition(V(MM(X0 + x), MM(Y0 + y))); t.SetTextSize(V(MM(sz), MM(sz))); t.SetTextThickness(MM(0.15))
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)

    # ---- screen module: three nested rectangles (outer -> inner) ----
    rect(0, 0, 120.7, 75.8, 0.30)                    # LCM outline  120.7 x 75.8  (OUTER module)
    rect(6.52, 2.42, 115.32, 68.62, 0.15)            # TP window    108.8 x 66.2  (touch glass viewable)
    rect(6.42, 3.32, 114.42, 68.12, 0.10)            # LCD A.A.     108.0 x 64.8  (active pixels)

    # ---- flex tails below the LCM bottom edge (local y measured from LCM top; bottom edge = 75.8) ----
    # From page 17: both tails are 53.2mm to their contacts. Display: bending(9.5) -> component/driver-IC
    # area (bottom at 34.43) -> neck -> 40P gold fingers (left 20.5, width 19.5) at the 53.2 tail end.
    B_ = 75.8
    rect(37.0, B_, 47.0, B_ + 9.5, 0.12)             # bending area (EMI bowtie, narrow)
    rect(26.4, B_ + 9.5, 49.0, B_ + 34.43, 0.12)     # component area: LCD driver IC + stiffener, H1.2 max (left=26.4)
    rect(36.0, B_ + 13, 48.0, B_ + 27, 0.10)         #   driver IC under insulation tape (right of component)
    rect(26.4, B_ + 34.43, 46.4, B_ + 49.7, 0.10)    # neck: component -> gold fingers
    rect(26.4, B_ + 49.7, 46.4, B_ + 53.2, 0.18)     # display 40P gold-finger block (LEFT=26.4, 40x0.5, at 53.2)
    rect(50.0, B_, 55.0, B_ + 49.7, 0.10)            # touch CTP narrow tail
    rect(49.0, B_ + 49.7, 56.0, B_ + 53.2, 0.18)     # touch 6P pin block (CTP tail 53.2 total)

    # pin ticks (thin rects): display 40x 0.5mm (contacts 0.3 wide), touch 6x 1.0mm (0.65 wide)
    for i in range(40):
        x = 26.4 + 0.5 + i * 0.5                      # 40 contacts, 0.5mm pitch, block left = 26.4
        rect(x - 0.15, B_ + 49.9, x + 0.15, B_ + 53.0, 0.05)
    for i in range(6):
        x = 49.0 + 1.0 + i * 1.0                      # 6 contacts, 1.0mm pitch (P1.0*5)
        rect(x - 0.32, B_ + 49.9, x + 0.32, B_ + 53.0, 0.05)

    # ---- dimension labels ----
    label(35, -3.5, "*120.7 (LCM outline)", 1.6)
    label(-16, 40, "75.8", 1.4)
    label(38, 8, "108.8 (TP window)", 1.2)
    label(38, 12, "108.0 (LCD A.A.)", 1.2)
    label(-16, 46, "66.2 / 64.8", 1.1)
    label(2, B_ + 51, "40P 0.5mm: connector left=26.4, tail 53.2", 1.1)
    label(57, B_ + 52, "6P 1.0mm (CTP tail 53.2)", 1.1)
    label(50, B_ + 22, "component/driver-IC body: left=26.4, bottom=34.43 (H1.2)", 1.0)
    label(37, B_ + 4, "bending area", 1.0)
    label(0, -8, "ATM0500D27 5\" TFT — mechanical (to scale) — VERIFY vs datasheet", 2.0)

    pcbnew.SaveBoard(PATH, B)
    print("screen ref drawn: LCM top-left (%.1f,%.1f), spans X[%.1f..%.1f] Y[%.1f..%.1f]"
          % (X0, Y0, X0, X0 + 120.7, Y0, Y0 + 75.8 + 53.2))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
