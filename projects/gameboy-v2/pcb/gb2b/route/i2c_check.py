#!/usr/bin/env python3
"""i2c_check.py — rise-time budget of gb2b's I2C1 bus (SCL / SDA), from the routed board.

Per line: C_bus = trace + vias + pins.
  trace  every segment's length x C' of a 0.15 mm trace on an outer layer over its plane with coplanar ground
         (the GND pours / neighbours, 0.15 mm gap: the conservative case), grounded-CPW closed form (Wadell)
  vias   0.45 pF each (0.45 / 0.25 through 1.6 mm, ~0.85 mm antipads, er 4.4)
  pins   datasheet input capacitance: STM32F411 C_IO 5 pF; PCA9555 C_i SCL 8 / C_io SDA 9.5 pF (max);
         DRV2605L and LSM6DSOX publish none -> 10 pF each (assumed)
Stackup: none is set in the board, so both JLC 4-layer 1.6 mm options are shown: JLC04161H-7628 (outer prepreg
0.2104 mm, er 4.4) and JLC04161H-3313 (0.0994 mm, er 4.1; thinner -> more C).
Rise time (30 % -> 70 %, as the I2C spec defines it) = 0.8473 R C; limits: Fast mode 300 ns, Standard 1000 ns;
R_min = (VDD - 0.4 V) / 3 mA.

Run:  <kicad python> route/i2c_check.py [board] [--rp 1500]
"""
import pcbnew, os, sys, math, csv
HERE = os.path.dirname(os.path.abspath(__file__)); TO = pcbnew.ToMM
args = [a for a in sys.argv[1:] if not a.startswith("--")]
RP = float(sys.argv[sys.argv.index("--rp") + 1]) if "--rp" in sys.argv else 1500.0
VDD = 3.3
B = pcbnew.LoadBoard(os.path.abspath(args[0]) if args else os.path.join(HERE, "..", "gb2b.kicad_pcb"))

def K(k):                                         # complete elliptic integral of the first kind (AGM)
    a, b = 1.0, math.sqrt(1 - k * k)
    for _ in range(40): a, b = (a + b) / 2, math.sqrt(a * b)
    return math.pi / (2 * a)
def cpwg_pf_per_mm(w, g, h, er):
    k = w / (w + 2 * g); kp = math.sqrt(1 - k * k)
    k3 = math.tanh(math.pi * w / (4 * h)) / math.tanh(math.pi * (w + 2 * g) / (4 * h)); k3p = math.sqrt(1 - k3 * k3)
    q = K(kp) * K(k3) / (K(k) * K(k3p))
    eeff = (1 + er * q) / (1 + q)
    z0 = 60 * math.pi / math.sqrt(eeff) / (K(k) / K(kp) + K(k3) / K(k3p))
    return math.sqrt(eeff) / (299792458.0 * z0) * 1e12 / 1e3   # pF/mm

PIN_PF = {("U1", "*"): 5.0, ("U11", "22"): 8.0, ("U11", "23"): 9.5, ("U8", "*"): 10.0, ("U7", "*"): 10.0}
def pin_pf(ref, pad):
    return PIN_PF.get((ref, pad), PIN_PF.get((ref, "*"), 0.0))
STACKS = {"JLC04161H-7628 (0.21 mm, er 4.4)": (0.2104, 4.4), "JLC04161H-3313 (0.10 mm, er 4.1)": (0.0994, 4.1)}
print(f"I2C1, pull-ups {RP:.0f} ohm to {VDD} V  (R_min for 3 mA: {(VDD - 0.4) / 3e-3:.0f} ohm)")
for net in ("I2C1_SCL", "I2C1_SDA"):
    L = {}; nv = 0; pins = []
    for t in B.GetTracks():
        if t.GetNetname() != net: continue
        if t.Type() == pcbnew.PCB_VIA_T: nv += 1; continue
        L[B.GetLayerName(t.GetLayer())] = L.get(B.GetLayerName(t.GetLayer()), 0.0) + TO(t.GetLength())
    for fp in B.GetFootprints():
        for p in fp.Pads():
            if p.GetNetname() == net: pins.append((fp.GetReference(), p.GetNumber()))
    cpin = sum(pin_pf(r, n) for r, n in pins)
    tot = sum(L.values())
    print(f"\n{net}: {tot:.0f} mm of track ({', '.join(f'{k} {v:.0f}' for k, v in L.items())}), {nv} vias, "
          f"pins {' '.join(r + '.' + n for r, n in pins)} = {cpin:.1f} pF")
    for name, (h, er) in STACKS.items():
        cpm = cpwg_pf_per_mm(0.15, 0.15, h, er)
        c = tot * cpm + nv * 0.45 + cpin
        tr = 0.8473 * RP * c * 1e-12 * 1e9
        rmax_fm = 300e-9 / (0.8473 * c * 1e-12)
        print(f"  {name}: {cpm:.3f} pF/mm -> trace {tot * cpm:.1f} + vias {nv * 0.45:.1f} + pins {cpin:.1f} = "
              f"{c:.1f} pF;  t_r = {tr:.0f} ns (Fast mode <= 300: {'OK' if tr <= 300 else 'FAIL'}, "
              f"margin {300 / tr:.1f}x);  Fast mode allows R up to {rmax_fm:.0f} ohm")
