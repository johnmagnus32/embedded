# Schematic input format: `<board>-bom.csv` + `<board>-nets.csv`

`generate_schematic.py` builds a complete, ERC-checked KiCad schematic from two CSVs — the **source of
truth** for the design. `bom.csv` says *what parts exist and where they go*; `nets.csv` says *what connects
to what*. They are joined on **`refdes`**.

```
generate_schematic.py --bom <board>-bom.csv --nets <board>-nets.csv --project <dir>
    one pass: group parts by subcircuit -> per subcircuit PLACE each part (spacing sized to fit its
      net-label text) + WIRE it (net labels / power symbols / PWR_FLAGs) -> one child sheet per
      subcircuit + a root
    then VALIDATE: every pin on a net + ERC clean + exported netlist matches the CSV
    (--subcircuit <name>… rebuilds just those sheets)
```

Both files are hand-edited (spreadsheet-friendly, one row per part / per pin). The format is **strict**:
the tool reads the columns below and ignores any extras (kept only for fab/human review), but the
**required** columns must be present and non-blank — there are no fallbacks and the tool never writes
the CSVs back.

---

## `bom.csv` — one row per physical part (every row IS placed)

| column | required | used by | meaning / allowed values |
|---|---|---|---|
| `refdes` | **yes** | tool (join key, symbol reference) | Schematic reference, e.g. `C1`, `U10`. **Non-blank, unique** — except that a multi-unit part repeats it once per unit (see `unit`). The stable key `nets.csv` joins on — the tool never invents it (a blank is a hard error). |
| `unit` | no | tool (multi-unit placement) | Which unit of a multi-unit symbol this row places, `1`..N. **Blank = every unit** of the symbol, all on this row's subcircuit — which for an ordinary single-unit part is simply the part. See *Multi-unit parts* below. |
| `subcircuit` | **yes** | tool (sheet grouping) | Which subcircuit (schematic page) the part goes on, e.g. `soc`, `core-buck`, `microsd`. One child sheet per distinct value. **Non-blank, no fallback** (a blank is a hard error). See *Designing subcircuits* below — this is your main lever for a readable schematic. |
| `symbol` | **yes** | tool (placement) | Library id `nick:name` to place, e.g. `easyeda2kicad:T113-S3_C5197687`. Must resolve in the symbol libraries. |
| `lcsc` | no | tool + fab | LCSC order code. Written as the placed symbol's `LCSC` field; also used by a fab/assembly BOM. |
| `note` | no | human | Free-form. The single human-readable column — part description, role, provenance, etc. |

Required columns (the tool errors if missing): **`refdes`, `subcircuit`, `symbol`** (`unit` is optional). There is no
`place` column — a part you don't want on the schematic simply isn't in this file. Footprints come from
the symbol definition, not the CSV. Any other columns (e.g. `footprint`, `fit`) are ignored.

### Designing subcircuits (how to get a readable schematic)

`subcircuit` is the **only** thing that decides which schematic page a part is drawn on: every part
sharing a value lands on `sheets/<board>-<subcircuit>.kicad_sch`, one page per distinct value. It is also
the unit of `--subcircuit` (rebuild just those pages) and the boundary for net labels — a net whose pins
span more than one subcircuit becomes a cross-sheet **global** label; a net confined to one subcircuit
stays a **local** label.

The tool auto-arranges parts *within* a sheet (a label-aware shelf pack — it can't know your intent), so
**readability is entirely down to how you group by `subcircuit`.** Put the parts that form one functional
block together — a regulator with its inductor / caps / feedback divider, a connector with its ESD +
pull-ups, an IC with its decoupling — under one short, meaningful name (`core-buck`, `microsd`, `usb-pd`,
`imu`). Keep each group focused (a handful up to a few dozen parts): too coarse and a page is an
unreadable wall of parts; too fine and parts that belong together scatter across pages. Each part is in
exactly one subcircuit.

### Multi-unit parts

A multi-unit symbol draws one physical part as several boxes (KiCad shows them as `U1A`, `U1B`, …) that
share one refdes, footprint and BOM line. To spread one across pages, give it **one row per unit**, each
with its own `subcircuit`:

```
refdes,unit,subcircuit,lcsc,symbol
U1,1,soc-core,C3345225,manual:RK3568B2      <- Part A: core power
U1,6,ddr,C3345225,manual:RK3568B2           <- Part F: DDR
U1,8,soc-io,C3345225,manual:RK3568B2        <- Part H: VCCIO1
U2,,ddr,C2831669,easyeda2kicad:MT41K512M16VRP-107IT_PTR   <- blank: both units on ddr
```

Rules (all hard errors): every unit of the symbol is placed exactly once; `(refdes, unit)` is unique; a
split part's rows agree on `symbol` and `lcsc`; a refdes with a blank-unit row has no other rows.
**`nets.csv` is unchanged** — pins are matched by number, which is unique across all of a part's units
(e.g. BGA balls), and the tool reads each pin's unit from the symbol. Net labels are decided per pin, so a
net between two units of one part on different pages correctly becomes a global label.

---

## `nets.csv` — one row per pin

| column | required | used by | meaning / allowed values |
|---|---|---|---|
| `refdes` | **yes** | tool (join key) | The part this pin belongs to; must match a `bom.csv` `refdes`. |
| `pin` | **yes** | tool | Pin **number** (as in the symbol), e.g. `1`, `80`. Pins are matched by number, not name. |
| `net` | **yes** | tool (connectivity) | The net this pin connects to, e.g. `+3V3`, `SD_CLK`. Special values: **blank** = intentional no-connect (a `no_connect` marker is drawn); **`TODO_*`** = not yet assigned (not rendered, and **fails validation**). |
| `kind` | for power | tool (symbol vs label) | Net classification — **the tool never guesses from the net name.** `gnd`, `pwr`, or blank/`sig` (signal). Must be **consistent across all rows of the same net** (the tool errors otherwise). |
| `note` | no | human | Free-form. The single human-readable column — pin function/name, rationale, etc. |

### What `kind` does
| `kind` | rendering |
|---|---|
| `gnd` | a `GND` power symbol at each pin, **plus one `PWR_FLAG`** on the net (marks it driven for ERC) |
| `pwr` | a rail power symbol (`power:<net>`) at each pin, **plus one `PWR_FLAG`** |
| `sig` (or blank) | a **net label** — *global* if the net spans more than one subcircuit (to cross child sheets), else a *local* label |

`PWR_FLAG`s are placed automatically for every `gnd`/`pwr` net (one per net, on the first child sheet),
so ERC's "power input not driven" is satisfied by declared data, not by name heuristics.

---

## Relationship + validation

- **Join:** every `nets.csv` `refdes` should be a part in `bom.csv`, and every part's pins should appear
  in `nets.csv`.
- The tool's mandatory **VALIDATE** gate fails the build unless:
  1. every pin has a net (no `TODO_*`; a blank net is an accepted intentional no-connect);
  2. ERC has **0 error-severity violations and 0 unconnected pins** (warnings are tolerated);
  3. every rendered net actually lands on its intended pin (exported-netlist check).

## Notes on pin electrical types (why ERC is clean)
Pin electrical types (`passive`/`input`/`output`/`bidirectional`/`power_in`…) are a property of the
**part**, so they live in the **symbol library** (`cad/symbols/…`), *not* in these CSVs — fix once, reuse on
every board. Passives (R/C/L/D/SW) are `passive`; IC signal pins should be typed by function. (`kind` in
`nets.csv` is different: it's a per-*net*, per-*design* property, so it correctly lives with the netlist.)
The two meet at `power_in` pins: ERC only counts a `PWR_FLAG` as their driver, and the tool places one
only on `gnd`/`pwr` nets — so **every net that reaches a `power_in` pin must be `kind` `gnd` or `pwr`**,
including a filtered rail on the far side of a ferrite bead (`manual:RK3568B2` types all 294 of its power
and ground balls `power_in`).
