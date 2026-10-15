"""gb2 rev B mechanical architecture: the z-stack model shared by every diagram in this folder.

Frame: z = 0 at the board's bottom face, +z toward the viewer (front). All numbers in mm.
Every layer carries a short source tag; SOURCES maps the tags to the file/page they come from.

Options:
  T  = panel TAPED to the board front (REVB_DESIGN.md baseline, t = 1.0)
  F  = panel FLOATING in the front shell 2.3 mm over the board so parts can stay under it (t = 1.6)
  R  = RECOMMENDED: panel hung from a 1.1 glass cover lens on a 0.3 gasket in the front shell, 0.3 over a zero-height
       board zone; cell on its own 0.5 stainless battery plate in the back shell (t = 1.2)
  R2 = R with the EastRising ER-TFT028A2-4 IPS panel (2.8 thick) instead of the 1774 (needs local plate thinning)
"""

SOURCES = {
    "MI-P5": "MI0283QT-11 datasheet P.5 (4.00 'Exclude tape', note 11 tol +/-0.20; CN4190 tape T=0.115)",
    "ER-p5": "ER-TFT028A2-4 datasheet p.5 (outline with FPC folded 50.20 x 69.3 x 2.8) and p.6 drawing (tape T=0.115)",
    "params": "enclosure/params.py:20 PLATE 1.6, :34 lens 1.5",
    "DESIGN": "enclosure/DESIGN.md:18,21 (Z_BACK = 1.7 part + 0.3 + 1.6 floor), :60-61 (0.5 worst-case lens gap)",
    "REVB": "pcb/REVB_DESIGN.md §3.1 (z stack), §3.5 (door 1.2, swell 0.85 assumed)",
    "PKCELL": "PKCELL LP-803860 spec p.5 (Max 62 x 38.5 x 8.5)",
    "task": "option (2) of the study brief: 2.3 mm under-panel gap (parts <= 2.0 + 0.3)",
    "SR5": "enclosure/STRUCTURAL_REVIEW.md #5 (1.1 glass is 8.6x stiffer than 1.5 acrylic)",
    "JLC": "jlcpcb.com/capabilities/pcb-capabilities (FR4 0.4/0.6/0.8/1.0/1.2/1.6/2.0; +/-10 % at >= 1.0 mm)",
    "board.json": "enclosure/build/board.json parts[].height (U12 1.7, SKRT 3.35, J7 3.25, L2 3.1)",
    "this": "this study (tolerance budget / sizing in me_numbers.py)",
}

# colours by kind
C = {
    "lens": "#bfe3f5", "glass": "#9fd0ea", "gap": "#ffffff", "gasket": "#444444", "panel": "#202020",
    "tape": "#c79a00", "board": "#2e7d32", "parts": "#9e9e9e", "plate": "#d6d6d6", "floor": "#d6d6d6",
    "cell": "#e8d44d", "swell": "#fff6c2", "door": "#b0bec5", "steel": "#78909c", "adh": "#8d6e63",
    "keys": "#e0e0e0",
}


def L(name, t, kind, src):
    return {"name": name, "t": round(t, 3), "kind": kind, "src": src}


def front_layers(opt):
    """Layers from the board top upward, through the screen centre. Returns (layers, front_above_board_top)."""
    if opt == "T":
        ls = [L("panel tape CN4190 (conductive)", 0.115, "tape", "MI-P5"),
              L("panel MI0283QT-11", 4.00, "panel", "MI-P5"),
              L("air gap 0.7 nom / 0.5 worst", 0.70, "gap", "DESIGN"),
              L("lens: cast acrylic", 1.50, "lens", "params")]
    elif opt == "F":
        ls = [L("parts under panel <= 2.0 + 0.3 clear.", 2.30, "parts", "task"),
              L("panel MI0283QT-11 (in shell frame)", 4.00, "panel", "MI-P5"),
              L("air gap 0.7 nom / 0.5 worst", 0.70, "gap", "DESIGN"),
              L("lens: cast acrylic", 1.50, "lens", "params")]
    elif opt in ("R", "R2"):
        pt = 4.00 if opt == "R" else 2.80
        pn = "panel MI0283QT-11" if opt == "R" else "panel ER-TFT028A2-4 IPS"
        ls = [L("back clearance (zero-height zone)", 0.30, "gap", "this"),
              L("panel tape, liner left on", 0.115, "tape", "MI-P5" if opt == "R" else "ER-p5"),
              L(pn, pt, "panel", "MI-P5" if opt == "R" else "ER-p5"),
              L("black foam gasket = air gap", 0.30, "gasket", "this"),
              L("cover lens: 1.1 strengthened glass", 1.10, "glass", "SR5")]
    else:
        raise ValueError(opt)
    return ls, round(sum(x["t"] for x in ls), 3)


def board_t(opt):
    return {"T": 1.0, "F": 1.6, "R": 1.2, "R2": 1.2}[opt]


def back_bay_layers(opt):
    """Layers from the board bottom DOWNWARD through the battery bay (listed board-side first)."""
    if opt in ("T", "F"):
        return [L("gap board-cell", 0.30, "gap", "REVB"),
                L("cell LP-803860 (8.5 max)", 8.50, "cell", "PKCELL"),
                L("swell space filled with foam", 0.85, "swell", "REVB"),
                L("battery door (PETG)", 1.20, "door", "REVB")]
    return [L("gap board-plate", 0.20, "gap", "this"),
            L("battery plate 0.5 stainless", 0.50, "steel", "this"),
            L("stretch-release tape", 0.15, "adh", "this"),
            L("cell LP-803860 (8.5 max)", 8.50, "cell", "PKCELL"),
            L("FREE swell gap (no foam)", 0.85, "swell", "REVB"),
            L("battery door (PETG)", 1.20, "door", "REVB")]


def back_grip_layers(opt):
    return [L("back parts <= 1.7 (U12)", 1.70, "parts", "board.json"),
            L("clearance", 0.30, "gap", "DESIGN"),
            L("back floor", 1.60, "floor", "DESIGN")]


def stack(opt, region):
    """Full stack bottom (back face) -> top (front face) for region in {'screen', 'grip', 'bay'}.
    Returns (layers_bottom_to_top, z_back, z_front)."""
    t = board_t(opt)
    fl, fh = front_layers(opt)
    back = back_grip_layers(opt) if region == "grip" else back_bay_layers(opt)
    z_back = -round(sum(x["t"] for x in back), 3)
    if region == "screen":
        front = fl
    else:  # outside the panel: face plate (1.6) over a free zone that holds keys / front parts
        front = [L("front zone: keys, parts <= %.2f" % (fh - 1.6 - 0.3), fh - 1.6, "keys", "this"),
                 L("face plate (PETG)", 1.6, "plate", "params")]
    layers = list(reversed(back)) + [L("PCB %.1f 4-layer" % t, t, "board", "JLC")] + front
    return layers, z_back, round(t + fh, 3)


OPTS = {"T": "TAPED (studies' baseline)", "F": "FLOATING over parts",
        "R": "RECOMMENDED: lens-hung panel + battery plate", "R2": "R with the 2.8 mm IPS panel"}
REGIONS = {"screen": "screen centre (x 97.9, y 71)", "grip": "grip (D-pad / U12 zone)",
           "bay": "battery bay outside the panel (y ~100)"}

if __name__ == "__main__":
    for o in OPTS:
        for r in REGIONS:
            ls, zb, zf = stack(o, r)
            print(f"{o:2s} {r:6s} back {zb:7.3f} front {zf:6.3f} total {zf - zb:6.3f}  "
                  f"sum {sum(x['t'] for x in ls):6.3f}")
