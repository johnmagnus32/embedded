#!/usr/bin/env python3
"""set_silk_text.py — set every silkscreen text object to one uniform size.

The board ships with KiCad's 1.0 mm / 0.15 mm default on all 186 refdes, which is oversized for a
0.4 mm-pitch LQFP board: the labels collide with each other and with pads once copper is poured.

Covers all three places text hides: footprint Reference/Value fields, extra graphical text inside
footprints (polarity marks, pin-1 labels), and board-level PCB_TEXT / PCB_TEXTBOX.

Absolute sizes, not a scale factor, so the script is idempotent — re-running never compounds.

  ⚠️ JLCPCB minimum silkscreen line width is 0.15 mm and minimum height 0.8 mm. Going below either
  gets the text thinned or dropped outright at fab. HEIGHT 0.8 / THICK 0.15 is the smallest
  combination that is still inside spec, which is why THICK stays at the default.

Set FAB_TOO = True to shrink the F.Fab/B.Fab value fields to match (they never reach the silkscreen,
but keeping them in step makes the fab drawing readable at the same zoom).

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 set_silk_text.py
"""
import pcbnew, os, sys

PATH = os.path.join(os.path.dirname(__file__), "gb3.kicad_pcb")
MM = pcbnew.FromMM; TO = pcbnew.ToMM

HEIGHT, WIDTH, THICK = 0.8, 0.8, 0.15      # mm — see the JLC note in the docstring
FAB_TOO = True                              # also resize the *.Fab value fields


def resize(t):
    t.SetTextHeight(MM(HEIGHT)); t.SetTextWidth(MM(WIDTH)); t.SetTextThickness(MM(THICK))


def main():
    B = pcbnew.LoadBoard(PATH)
    silk = {B.GetLayerID("F.Silkscreen"), B.GetLayerID("B.Silkscreen")}
    fab = {B.GetLayerID("F.Fab"), B.GetLayerID("B.Fab")}
    want = silk | fab if FAB_TOO else silk

    n_field = n_fpgfx = n_board = 0
    for fp in B.GetFootprints():
        for t in (fp.Reference(), fp.Value()):
            if t.GetLayer() in want:
                resize(t); n_field += 1
        for g in fp.GraphicalItems():                  # extra text baked into the footprint
            if g.GetClass() in ("FP_TEXT", "PCB_TEXT", "PCB_TEXTBOX") and g.GetLayer() in want:
                resize(g); n_fpgfx += 1

    for d in B.GetDrawings():                          # free-standing board text
        if d.GetClass() in ("PCB_TEXT", "PCB_TEXTBOX") and d.GetLayer() in want:
            resize(d); n_board += 1

    pcbnew.SaveBoard(PATH, B)
    print("set %.2f x %.2f mm, %.2f mm stroke%s" %
          (HEIGHT, WIDTH, THICK, "  (silk + fab)" if FAB_TOO else "  (silk only)"))
    print("  %d ref/value fields, %d footprint graphic texts, %d board texts" %
          (n_field, n_fpgfx, n_board))
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
