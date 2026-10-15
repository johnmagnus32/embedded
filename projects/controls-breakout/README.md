# controls-breakout — mechanical de-risk board for gameboy-v4 controls

**Status:** 🔬 Spec only. Nothing designed, ordered or built.

**Purpose: de-risk the *mechanical* half of [gameboy-v4](../gameboy-v4/pcb/BOM.md)'s controls** — silicone
membrane buttons and the analog sticks. Those are the only parts of the v4 board whose correctness cannot
be established from a datasheet, because they depend on **enclosure geometry**: whether the shell holds
the rubber at the right compression, whether pads line up with the carbon pills, whether a stick's throw
clears its opening.

**It replaces the retired `rk3568-breakout`.** That board existed to de-risk SoC + DDR3 + PMIC + boot — but
v4 carries all of that anyway, the peripherals are low-risk buses, and a respin of the full board is not
much more expensive than a bare one. The risk that *is* worth a separate cheap board is mechanical, and
mechanical risk is what this one tests.

**Deliberately trivial electrically.** No SoC, no regulators, no firmware. If it needs debugging, it has
failed at its job.

---

## 1. What goes on it

| Block | Contents | Why |
|---|---|---|
| **Membrane buttons** | exposed copper pad pairs for **D-pad + A/B/X/Y + L/R + ZL/ZR**, **in 3-4 competing pad geometries** | the whole point |
| **Button readout** | 1 LED + series resistor per pad pair, straight off 3.3 V | **zero firmware** — press it, it lights. Iterate shells all afternoon with no toolchain in the loop |
| **Shoulder levers** | pad pairs for L/R + ZL/ZR, with printed pivoting levers | ⛔ **Nintendo uses membrane here too** — the lever pivot is its own mechanical unknown |
| **Analog stick ×2** | `FPC-0.5FX-05PH20` (LCSC `C19077699`, 5P 0.5 mm, double-sided contacts) + 2:1 dividers | an analog axis can't usefully drive an LED |
| **Stick readout** | `X`/`Y`/`S` per stick to a 1×8 header | wire a Pico → live numbers, and it closes v4's open **output-range** question |
| **Power** | 2-pin header (bench supply) or the Pico's 3.3 V | no regulator, no USB-C |

2-layer, roughly 40 × 70 mm. Five boards should be **under $20**.

## 2. ⛔ Three non-negotiables

1. **Order it in ENIG, not HASL.** The finish is itself under test. A builder who tried HASL for membrane
   contacts found it *"very sensitive to dust"*; the consensus is *"ENIG because it's flat and remains
   conductive."* A HASL test board would validate a stack we are not going to build. (v4 needs ENIG for the
   same reason, so this also confirms the upcharge is buying something.)
2. **Match the v4 face geometry exactly** — button centres, stick centres, spacing. Then the shell printed
   for this board **is** a prototype v4 face plate, and the standoff height transfers directly. Otherwise
   we are measuring a geometry we do not intend to build.
3. **Order it only after the rubber and caps arrive.** Pad centres must match the pills we measured, not a
   guess. That is a week of waiting versus a respin.

## 3. Pad geometry — test several at once

The open question is shape and fineness, and it costs nothing to settle empirically. Put each variant
under the same rubber on one board and pick the winner by pressing it:

| Variant | Rationale |
|---|---|
| **Interdigitated, fab-minimum fingers** | the recommendation — *"as tight as your PCB fab capabilities allow"*; most crossings survive pill wear |
| **Interdigitated, ~2× coarser** | does finer actually help, or just cost yield? |
| **Concentric rings** | common in production; tolerant of off-centre presses |
| **Split semicircles** | simplest; the baseline to beat |

No solder mask over any of them.

## 4. Parts

| Role | Part | LCSC | Qty |
|---|---|---|---|
| Stick FFC connector | **FPC-0.5FX-05PH20** — 5P 0.5 mm, double-sided contacts | **C19077699** | 2 |
| Divider resistors | 10 kΩ 0402 (2:1 on each axis — safe for any 0-3.6 V output) | C25744 | 8 |
| Button LEDs | any 0603 | — | ~12 |
| LED resistors | 1 kΩ 0402 | C11702 | ~12 |
| Headers | 1×8 (stick), 1×2 (power) | — | 3 |
| **Silicone membrane** | Switch **Lite** conductive rubber set, **$7.99** — buy **3** (one to destroy and measure, one for this board, one for v4) | Amazon `B0CYSR7XLV` | 3 |
| **Plastic caps** | Switch Lite replacement button set | Amazon | 1 |
| Analog sticks | TMR Hall Joy-Con replacement (+ carbon-film pack as measuring stock) | Amazon | 2 |

✅ **What the Switch Lite set actually contains** (read off the product photographs, so this is settled
rather than assumed): a **one-piece D-pad cross with hinge slots** at its centre · a **4-dome A/B/X/Y
diamond** with a central locating hole · **2 square pads with raised square centres — the L/R shoulders** ·
**2 round dark-centred domes — the ZL/ZR triggers** · a **4-dome Home/Capture/Plus/Minus strip** · a clear
plastic power-button piece.

⛔ **So Nintendo uses membrane for the shoulders too**, which resolves a question that conflicting text
sources could not (a GBAtemp thread referenced "Joy Con ZL tactile switch specs"; eXtremeRate describe the
ZL/ZR assembly as conductive rubber domes). It also means **this board must test the shoulders**, not just
the face buttons — and that the shoulder *lever* geometry is a mechanical unknown of the same kind as
membrane compression.

⛔ **Buying a donor membrane inherits its button spacing** — the rubber is molded one piece per group, so
the ABXY cluster, the D-pad cross and the shoulder pads all come from Nintendo's design, and our pads *and*
v4's must match it. This is mostly a gift (the ergonomics are solved) but it is a hard layout constraint.

## 5. Measurement protocol

**Before designing the PCB** — from the rubber in hand:
- pill **centre-to-centre spacing** per group → pad centres
- pill **diameter** → pad diameter (slightly smaller)
- **uncompressed dome height** and travel before the pill lands → the shell standoff
- whether the set covers shoulder buttons

**After the boards arrive** — iterate printed shells at several standoff heights:
- lowest standoff where every button actuates reliably, and highest before any button self-presses
- which pad geometry works across the widest standoff range *(the real figure of merit — tolerance, not
  just "does it work")*
- D-pad: does one rubber cross on a pivot over 4 pads give clean cardinal presses without diagonals?
- **shoulders — the second mechanical unknown:** lever **pivot position**, travel before the pad contacts,
  pre-load at rest (must not self-press), and whether an angled finger push actuates reliably across the
  whole lever face rather than only near the tip
- cap fit and wobble through the shell openings
- sticks: FFC seats, 2 screw bosses align, shaft clears its opening through full 360° throw, gasket seats
- **stick output range** per axis at centre and both extremes → confirms the v4 divider ratio

## 6. What this answers that nothing else can

Everything here is geometric. None of it is knowable from a datasheet, and all of it is cheap to iterate on
a $4 PCB plus a 3D print.

- the shell-to-PCB **standoff** for correct compression ← the critical unknown
- **shoulder lever pivot geometry** — travel, pre-load, actuation across the lever face
- whether our pads line up with the pills
- which **pad geometry** is most tolerant
- **D-pad** feel on a pivot over 4 pads
- **ENIG** contact reliability in practice
- stick FFC fit, screw bosses, throw clearance, gasket
- cap fit through shell openings

**Fully parallel to v4.** Nothing here blocks the SoC, DDR3, PMIC, radio or display work — this board can
be ordered while the v4 BOM is still settling.

## 7. Open

- 🔲 Pad dimensions — blocked on measuring the rubber
- 🔲 Whether to carry **tactile footprints on v4 spin 1** as a fallback. Pads and tactile footprints cannot
  share the same area, so it is one or the other per spin. If this board validates the membrane stack
  first, v4 can commit to membranes with no hedge — **which is the main reason to build this board before
  v4's layout freezes.** ⚠️ This matters more now that the shoulders are membrane too: with only volume
  left tactile, **v4 has essentially no buttons testable without its enclosure**, so this board carries the
  entire button risk.
