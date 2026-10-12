# gb2 routing plan

How the board gets routed, in order, and the rules every route group follows. The reserved corridors are in
`channels.py` and are drawn with `route/plot.py ... --channels`.

## Stackup and layer rules

| Layer | Role |
|---|---|
| F.Cu | Components; short fan-outs; the display bus; the PPU lane; the bottom-edge channel E2; switcher GND pours |
| In1.Cu | Solid GND. Never cut. |
| In2.Cu | +3V3 plane, with the +1V2 island under the FPGA core (59.7–74 × 57–61) |
| B.Cu | Back-side passives; long east/west channels (W1, E1); power trunks that have to cross a front trunk |

- Fast or edge-rich signals (display bus, PPU SPI, SPI3, I2S, crystal) stay on F.Cu over the In1 GND plane.
- B.Cu signals reference In2, so they never cross the +1V2 island's edge.
- The crystal (Y1) area carries no other copper on F.Cu. Nothing runs under the crystal on B.Cu either (the
  battery-sense tap used to; it now runs under J7).

## The two choke points

The 2090 screen sits on two 20-pin headers, J3 (x 57.15) and J4 (x 133.35). Both columns are closed at both ends
by screw and standoff keep-outs. Anything going to the left grip crosses J3 between its pins, and anything going
to the right grip crosses J4 between its pins.

- J3/J4 use `manual:PinSocket_1x20_P2.54_D1.0`: 1.6 mm pads on 1.0 mm drills, the KiCad PinSocket drill for
  0.64 mm square tails. It was 1.8/1.2. The between-pin gap grows from 0.74 to 0.94 mm, so **two traces fit per
  gap per layer** (0.15/0.15, or one 0.5 mm power neck).
- **J3, west (W1, back):** BT ×5, PWR_SW and I2C ×2 cross in the four gaps above the FPGA's back-side decaps
  (y 48.1 / 50.7 / 53.2 / 55.8). On the front, J3's east side is the display-bus fan, so a front-layer crossing
  would cut a data line. FB2 is kept out of this strip (`place_fpga` `in_lane`).
- **J4, east (E1 back, E2 front, A):** 8 signals to the right grip (buttons, I2S), plus the amp's SYS feed in the
  top gap (y 48.1). On 2026-10-12 the console header J2 there became back-side test pads under the MCU and the SWD header
  J1 moved to the top edge east of the USB-C (`65_debug`), which took five lines off this crossing. SWD reaches J1
  east of the BAT trunk: two back lanes below E1, up through J8's BAT|GND pad gap on the front.

## Channels (route/channels.py)

| Ch | Layer | Carries |
|---|---|---|
| W1 | B | BT ×5 and PWR_SW from the MCU's top-left; I2C ×2 from under the MCU → J3 gaps → left grip |
| W2 | F | LCD_CS/DC/WR/RST from U2's bottom pins down J3's east side |
| W2b | B | LCD_LITE from the MCU's bottom pins under U2 into the same strip |
| P | F | SPI1 SCK/MOSI/select to U2's config pins (configuration + PPU commands, one bus), CRESET_B, CDONE |
| H | B | Under the MCU body: BT_EN, I2C ×2 west |
| E1 | B | BTN_A, VOL±, BTN_R → J4 gaps 50.7 / 53.2 / 55.8 |
| E2 | F | I2S ×3, BTN_B along the bottom edge under NOR and J8 → J4 gaps 60.7–63.6. The amp takes the I2S lines in the opposite rotation to the MCU's pins, so WS and CK dive to the back west of J4 (WS first, which also swaps them): 4 vias in all |
| A | B | SYS to the amp: Q1 → y 45.2 → J4 gap 48.1 (0.5 mm neck) → across the right grip → C30 |
| N | B | Under J7, between its shell-tab rows: BAT (sense tap), /PGOOD, /CHG → MCU top pins / R26 |

Fanout vias and test points stay out of the channels. A pad with no other fit, or one inside a channel because
its part sits there, may break this rule; the fanout run lists them.

## Power budget (route/ipc_check.py)

Worst cases: SYS, SYS_SRC and BAT 1.5 A (amp peaks + buck-boost input at 3.0 V); amp feed 1.0 A; VBUS 0.5 A
(ILIM 3.3 kΩ); BB_L1/L2 1.8 A; +3V3 0.6 A; +5V_LCD 0.15 A.

Every trunk passes the IPC-2221 curve at a 10 °C rise on 1 oz outer copper. Every layer change on a trunk has
enough 0.25/0.3 mm vias in parallel. Copper under 0.4 mm on a power net is a signal-current branch only (EN tie,
sense tap, Miller tie, VINA/VDD pins, test-point stubs), and the check lists those for review. The amp feed drops
about 60 mV at a 1 A peak.

## Signal integrity notes

- **One SPI1 bus to the FPGA (2026-10-12):** the STM32 configures the FPGA through its config pins (SPI_SCK 15 /
  SPI_SI 17 / SPI_SS 16) and the PPU gateware reuses those pins as its command bus (datasheet §5.1.3: they're
  general I/O in user mode). The runtime select is active-high, so it idles low with the 3.3 kΩ SS_B strap. SPI3
  serves only the NOR: point-to-point, about 15 mm, no termination needed. Firmware runs SCK/MOSI at OSPEEDR
  "medium" (about 10 ns edges) for margin and less EMI.
- **Return paths:** a front-to-back via moves a signal's reference from In1 (GND) to In2 (+3V3). Near the MCU and
  FPGA the decaps carry the return across; on the long W1/E1 runs each cluster of signal vias gets a GND stitching
  via beside it.
- **I2C** spans the board (IMU east; expander and haptics far west). Its capacitance and rise time get checked
  against the pull-ups once it's routed.

## Grounding

- Exposed pads have thermal-via grids (`place.py --thermal-vias`). The grid steps around the decap GND pads that
  sit under it.
- The FPGA's back-side decaps have their rail pad under the pin and the GND pad inward under the exposed pad,
  strapped into the EP's vias. The rail pad reaches the pin through one via straight out along the pin's own lane.
- Switcher hot loops close on F.Cu as well: there is a GND pour under the LCD boost + buck-boost, the charger and
  the amp, and each switcher cap's GND pad gets **two** vias of its own.
- Fanout gives every other plane pad one 0.45/0.25 via. On big ICs it sits straight out along the pin axis (in the
  power pin's own lane), else under the body. A short tie to on-plane copper beats a via inside a channel.

## Order (route/groups/, built by route.py, then fanout.py)

1. `10_power`: power chain trunks.
2. `15_switcher_gnd`: F.Cu GND pours under the switchers.
3. `20_charger`: charger/USB signals (channel N).
4. `30_fpga_power`: U2's west-side supply pins and decaps.
5. `55_mcu_local` (crystal, +3V3A filter, VCAP1, BB_PSYNC), `65_debug`, `70_east`, `75_right_grip` are done.
   **Next:** the display bus + LCD control (W2/W2b); the PPU lane (P); SPI3 + NOR (S, H); the MCU's right side and
   bottom → E1 / E2 → right grip; W1 → left grip (BT, I2C → U11 / U8, PWR_SW → SW1); crystal; haptics; the rest.
6. Last: whole-board GND pours on F.Cu/B.Cu with edge and field stitching; silk cleanup; final DRC.
