#!/usr/bin/env python3
"""fab.py — gb3's fabrication + assembly outputs for JLCPCB, and the pre-fab footprint checklist (ported from gb2's).

Writes (into fab/out/, regenerated every run; not committed):
  gerbers/            F/In1/In2/B copper, paste, mask, silk, Edge.Cuts (Protel extensions, X2, silk minus mask)
                      + Excellon drills (PTH / NPTH separate, mm, absolute origin) + drill map PDF
  gb3-gerbers.zip     the above, zipped for upload
  gb3-bom-jlc.csv     JLC assembly BOM: Comment, Designator, Footprint, LCSC Part #  (BOM.md "Fit" = JLC only)
  gb3-cpl-jlc.csv     JLC placement: Designator, Mid X, Mid Y, Layer, Rotation  (the same parts)
  gb3-hand-parts.csv  everything fitted by hand (BOM.md "Fit" = Hand)
and fab/FOOTPRINT_CHECKLIST.md (committed): one row per part line, side, the pin-1 / polarity / orientation check.
LCSC numbers come from gb3-bom.csv (the footprints' own fields are the library's and wrong). Never use the
Fabrication Toolkit button: it writes bottom-side rotations JLC reads differently.

Run:  python3 fab/fab.py        (needs kicad-cli; run after route.py -> fanout.py -> pour.py -> silk.py, board saved)
"""
import os, sys, csv, re, subprocess, zipfile, collections, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); BRD = os.path.join(HERE, "..")
PCB = os.path.join(BRD, "gb3.kicad_pcb"); OUT = os.path.join(HERE, "out")
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
LAYERS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts"

def run(*a):
    r = subprocess.run([CLI, *a], capture_output=True, text=True)
    if r.returncode: sys.exit(f"kicad-cli {' '.join(a[:3])} failed:\n{r.stdout}{r.stderr}")
shutil.rmtree(OUT, ignore_errors=True); g = os.path.join(OUT, "gerbers"); os.makedirs(g)
run("pcb", "export", "gerbers", "-o", g + "/", "-l", LAYERS, "--subtract-soldermask", "--check-zones", PCB)
run("pcb", "export", "drill", "-o", g + "/", "--format", "excellon", "--excellon-units", "mm",
    "--excellon-zeros-format", "decimal", "--excellon-separate-th", "--generate-map", "--map-format", "pdf", PCB)
with zipfile.ZipFile(os.path.join(OUT, "gb3-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(os.listdir(g)):
        if not f.endswith(".pdf"): z.write(os.path.join(g, f), f)
pos = os.path.join(OUT, "pos.csv")
run("pcb", "export", "pos", "-o", pos, "--side", "both", "--format", "csv", "--units", "mm", PCB)

# ---- BOM.md: part rows by index (#): | # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
rows = {}
for l in open(os.path.join(BRD, "..", "BOM.md"), encoding="utf-8"):
    c = [x.strip() for x in l.strip().strip("|").split("|")]
    if l.startswith("| ") and c[0].isdigit() and len(c) == 9: rows[int(c[0])] = c
bom = list(csv.DictReader(open(os.path.join(BRD, "gb3-bom.csv"), encoding="utf-8")))
def idx(note):                     # "... · #N" (some notes go on after it: " · #9 · multi-part row ...")
    m = re.search(r"(?:^|·)\s*#(\d+)\b", note); return int(m.group(1)) if m else None
def clean(part): return re.sub(r"[*`~]", "", re.sub(r"\s*·.*", "", part)).strip()
def comment(part, lcsc):           # a multi-part row ("A (C1) + B (C2)") -> the piece that names this LCSC code
    for piece in re.split(r"\s+\+\s+", part):
        if "(" + lcsc + ")" in piece: return clean(piece.replace("(" + lcsc + ")", "")), True
    return clean(part), False
placed = {r["Ref"]: r for r in csv.DictReader(open(pos, encoding="utf-8"))}
jlc, hand, problems = collections.OrderedDict(), [], []
for b in bom:
    ref = b["refdes"]; i = idx(b["note"]); row = rows.get(i)
    src = row[7] if row else "?"; part = row[3] if row else b["note"]
    if ref not in placed: problems.append(f"{ref}: not on the board"); continue
    if src == "JLC":
        if not b["lcsc"].startswith("C"): problems.append(f"{ref}: JLC part with no LCSC number"); continue
        c, multi = comment(part, b["lcsc"])
        e = jlc.setdefault(b["lcsc"], {"comment": c, "multi": multi, "refs": []}); e["refs"].append(ref)
        if e["multi"] and not multi and row: e.update(comment=c, multi=False)   # prefer the code's own row's name
    elif src == "Hand":
        hand.append((ref, clean(part), row[4] if row else "", placed[ref]["Side"]))
    elif src != "—":
        problems.append(f"{ref}: BOM.md row #{i} has no Fit (JLC / Hand / —)")
def refkey(r): return (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0))
with open(os.path.join(OUT, "gb3-bom-jlc.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for lc, e in jlc.items(): w.writerow([e["comment"], ",".join(sorted(e["refs"], key=refkey)), placed[e["refs"][0]]["Package"], lc])
with open(os.path.join(OUT, "gb3-cpl-jlc.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for e in jlc.values():
        for r in sorted(e["refs"], key=refkey):
            p = placed[r]
            # JLC reads Rotation as seen from above the part, so a bottom-side angle is mirrored to 180 - θ (Fabrication
            # Toolkit process.py, Bouni's kicad-jlcpcb-tools fix_rotation); kicad-cli writes θ as the board shows it
            rot = "%.6f" % ((float(p["Rot"]) if p["Side"] == "top" else 180.0 - float(p["Rot"])) % 360.0)   # 0 .. 360
            w.writerow([r, p["PosX"] + "mm", p["PosY"] + "mm", "Top" if p["Side"] == "top" else "Bottom", rot])
with open(os.path.join(OUT, "gb3-hand-parts.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Designator", "Part", "Source", "Side"]); w.writerows(hand)
os.remove(pos)
side = collections.Counter(placed[r]["Side"] for e in jlc.values() for r in e["refs"])
print(f"gerbers: {len(os.listdir(g))} files -> out/gb3-gerbers.zip")
print(f"JLC assembly: {sum(len(e['refs']) for e in jlc.values())} parts, {len(jlc)} LCSC lines (top {side['top']}, bottom {side['bottom']})")
print(f"hand-fit: {len(hand)} parts ({', '.join(r for r, *_ in hand)})")
for p in problems: print("  PROBLEM:", p)

# ======================================================================== footprint checklist
SPECIAL = {
    "U1": "T113-S3: pin-1 corner, 0.4 mm pitch, the exposed pad's 4 x 4 via grid (epoxy-filled + capped with the rest); land pattern vs the datasheet's.",
    "USBC1": "USB-C TYPE-C-31-M-12: the mouth faces out of the top edge. Its land pattern puts the shell tabs 0.35 mm from the edge and A1B12 / B1A12 0.235 mm from the pegs (DRU exception at exactly those values).",
    "CN1": "**Battery JST-PH:** pin 1 = BAT (+). Check against the cell's leads (red = +) before the first plug-in; swap the crimps if reversed, no board change.",
    "X1": "24 MHz 3225 crystal: pins 1 / 3 are the crystal, 2 / 4 GND. Check the footprint's numbering against the datasheet (a 90 deg error shorts the oscillator).",
    "X2": "32.768 kHz SC-20S: no copper between the pads (rule area); pad 1 = RTC_IN.",
    "P1": "Display FPC (40 pin): the flex's pin 1 (VLED-) lands on the west end (the silk dot; the footprint's own mark, at pad 40, is on Fab); the contact side against the ATM0500D27's flex as it folds behind the panel.",
    "FPC1": "Touch FFC (6 pin, 1.0 mm, B side): rotated 180 deg before fab, so the slot faces the board's bottom edge and the tail arrives round the notch edge (y 111). Pads are numbered 6..1 west to east: pad 6 = VDD = the tail's pin 1 (x 116), 5 GND, 4 SCL, 3 SDA, 2 INT, 1 RST (x 121). Before ordering: wrap the real tail (or a 53.4 mm paper strip from the glass edge) round a 1.6 mm board edge at the notch; its end must reach y 72.2 (4.1 inside the lid at y 76.3).",
    "U17": "STM6601 TDFN-12: pin 1; SR (pin 2) is tied to /PB and C_SRD (pin 5) to C97 (0.47 uF, F side) through the two 0.5 / 0.3 vias between its pad rows: holding the power button 3.5-7 s forces EN off.",
    "Audio1": "3.5 mm jack: the barrel overhangs the bottom edge (its silk is on Fab); tip / ring / sleeve / switch against the datasheet.",
    "SW2": "TS-1187A: pads 1 / 2 and 3 / 4 are joined inside (jumper pad groups). The keymap follows the switch positions (BOM.md, routing step 4).",
    "SW12": "ALPS SKRT side switch: the actuator faces out of the board edge; the no-copper area between the lugs is a keep-out.",
    "D4": "USBLC6-2SC6: pin 1 orientation (I/O1 = D-, I/O2 = D+).",
    "Q1": "SOT-23 FET (the ILIM switch): G / S / D against the datasheet.",
    "J2": "LRA wire pads (an LRA has no polarity).", "J3": "Speaker wire pads: SPK_P on pad 1.",
    "H1": "M2 mounting holes (NPTH, board-only, no part): H3 2.2 mm is the datum, H1 a 2.2 x 4.4 mm slot aimed at H3, H2 / H4 / H5 2.8 mm (they absorb the printed shell's scale error). Check H1's slot in JLC's drill preview.",
    "U18": "BT830 (hand-fit, back): the antenna end in the top-left keep-out (moved 3.5 mm down in step 8 for the R20 corner); pin 1 (the silk dot below it); castellated pads, no copper under the body.",
    "U4": "microSD socket (A-MicroTF-1.85A): the card slot faces the board edge; its two prohibition bars are copper keep-outs.",
    "U5": "0603 LED: cathode mark against pad 1 (the footprint's polarity) in JLC's preview.",
}
ALIAS = {"U14": "U5", **{f"SW{k}": "SW2" for k in (1, 3, 4, 5, 6, 7, 8, 9, 10, 11)}, **{f"SW{k}": "SW12" for k in (13, 14, 15, 16)},
         **{f"H{k}": "H1" for k in (2, 3, 4, 5)}}
def kind(ref):
    p = re.sub(r"\d", "", ref)
    return {"U": 0, "X": 1, "Q": 1, "D": 1, "USBC": 2, "CN": 2, "P": 2, "FPC": 2, "J": 2, "Audio": 2, "SW": 3, "L": 4,
            "R": 6, "C": 6, "TP": 7, "H": 8}.get(p, 4)
GENERIC = {0: "IC: pin-1 corner + land pattern (pad size / pitch / EP) against the datasheet's recommended footprint",
           1: "polarized / pinout-sensitive: pin numbering against the datasheet", 2: "connector: pinout, orientation, mating side",
           3: "switch: footprint against the datasheet, actuator direction", 4: "size against the datasheet",
           6: "standard chip part", 7: "bare test pad, nothing assembled", 8: "no part"}
lines = collections.OrderedDict()
for b in bom:
    ref = b["refdes"]; i = idx(b["note"]); row = rows.get(i)
    if ref not in placed: continue
    key = (b["symbol"], b["lcsc"], i)
    e = lines.setdefault(key, {"refs": [], "part": row[3] if row else "", "src": row[7] if row else "?", "side": set(), "k": 9})
    e["refs"].append(ref); e["side"].add(placed[ref]["Side"]); e["k"] = min(e["k"], kind(ref))
with open(os.path.join(HERE, "FOOTPRINT_CHECKLIST.md"), "w", encoding="utf-8") as f:
    f.write("# gb3 pre-fab footprint checklist\n\n"
            "Generated by `fab/fab.py` from gb3-bom.csv, BOM.md and the board. Wrong footprints, not routing, are what\n"
            "usually kills a first spin, so check each line against the part's datasheet before ordering. The fastest way:\n"
            "print the F.Fab / B.Fab layers 1:1 (KiCad: File > Print, 1:1 scale, check a 10 mm dimension with a ruler) and lay\n"
            "the real connectors, switches, the panel's flexes and the battery on the paper. Nothing here is verified yet.\n\n"
            "Highest risk first. LCSC links go to the part page (datasheet PDF linked there).\n\n"
            "| ✓ | Refs | Part | Footprint | Side | Fit | What to check |\n|---|---|---|---|---|---|---|\n")
    for (sym, lc, i), e in sorted(lines.items(), key=lambda kv: (kv[1]["k"], refkey(kv[1]["refs"][0]))):
        refs = ", ".join(sorted(e["refs"], key=refkey))
        notes = list(dict.fromkeys(SPECIAL[ALIAS.get(r, r)] for r in e["refs"] if ALIAS.get(r, r) in SPECIAL)) or [GENERIC.get(e["k"], "")]
        part = clean(e["part"]).replace("|", "/")
        link = f"[{lc}](https://www.lcsc.com/product-detail/{lc}.html)" if lc.startswith("C") else "—"
        f.write(f"| ☐ | {refs} | {part} ({link}) | `{placed[e['refs'][0]]['Package']}` | {'/'.join(sorted(e['side']))} | {e['src']} | {' '.join(notes)} |\n")
    f.write("\n## Assembly files (fab/out/, check in JLC's online preview)\n\n"
            "- **Rotations:** easyeda2kicad footprints come from the same EasyEDA library JLC's placement uses, so most need no\n"
            "  offset, but check every polarized part (ICs, diodes, LEDs, Q1, the crystals) in JLC's 3D / 2D preview, and\n"
            f"  especially the {side['bottom']} **bottom-side** parts: their CPL angle is written as 180 - θ (JLC's 'seen from above\n"
            "  the part' convention).\n"
            "- **Board:** 4 layers, 1.6 mm, stackup **JLC04161H-7628** (the impedance and I2C estimates assume it); 1 oz outer;\n"
            "  min track / space 0.15 / 0.15 mm; vias 0.6 / 0.3 mm (0.5 / 0.3 at U1.27 and between U17's pad rows); **Via Covering:\n"
            "  Epoxy Filled & Capped, all vias** (39 exposed-pad vias sit in pad openings: U1 16, U12 8, U10 5, U9 4, U20 4, U13 2;\n"
            "  JLC fills 0.15-0.55 mm holes); ENIG (U1's 0.4 mm pitch, the 0.5 mm-pitch QFNs).\n"
            "- **Mounting holes (NPTH):** H3 2.2 mm is the datum, H1 a 2.2 x 4.4 mm slot aimed at H3, H2 / H4 / H5 2.8 mm (they\n"
            "  absorb the printed shell's scale error): check H1's slot in JLC's drill preview.\n"
            "- **LCM ground contacts:** three 4 x 3 mm F-mask openings over the GND pour under the screen's top edge (bare ENIG,\n"
            "  no paste): conductive foam to the LCM frame goes there at assembly.\n"
            "- **Panel:** the outline has cut-outs and parts over both long edges (USBC1, Audio1): let JLC add the rails, fiducials\n"
            "  and tooling holes (expect a DFM query), with a routed relief, not V-score, in front of USBC1 and Audio1.\n"
            "- **USBC1's shell legs** carry B.Paste apertures (pin-in-paste): ask JLC in the order notes to make sure the four legs\n"
            "  are soldered, and check them at incoming inspection (hand-solder from the F side if not).\n"
            f"- **Double-sided assembly** (back: U1 and most of the board; front: U1's decap island, the buttons, the jack,\n"
            f"  P1, CN1, the LEDs). Hand-fit after: {', '.join(f'{r} ({p})' for r, p, *_ in hand)}.\n")
print("checklist: fab/FOOTPRINT_CHECKLIST.md,", len(lines), "lines")
