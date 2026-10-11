# U2's west side (the display-bus side): its three supply pins and the four decaps under them on the back, done by
# hand because the 3 mm fan from U2 to J3 runs right here on the front. Each supply pin gets one 0.45 via straight out
# along its own pin axis (its lane carries no signal; staggered 29 / 30 so both fit at 0.5 mm pitch), and the decaps
# reach those vias with short back-side stubs. Their GND pads sit inward under the exposed pad and are strapped
# together into the EP's thermal vias - no GND via in the fan at all.
pll, core, io = (59.95, 59.34), (60.5, 59.84), (60.6, 61.34)
# +1V2_PLL: pin 29 -> via -> C19 / C20 (stacked) <- FB2.2
T("+1V2_PLL", "F", 0.2, [P("U2.29"), pll]); VIA("+1V2_PLL", *pll, d=0.45, h=0.25)
T("+1V2_PLL", "B", 0.2, [pll, P("C19.1")])
T("+1V2_PLL", "B", 0.3, [P("C19.1"), P("C20.1"), P("FB2.2")])
# +1V2 core: pin 30 -> via (lands on the +1V2 island) -> C14; FB2.1 (PLL filter input) round below the PLL via
T("+1V2", "F", 0.2, [P("U2.30"), core]); VIA("+1V2", *core, d=0.45, h=0.25)
T("+1V2", "B", 0.3, [core, (60.5, 60.59), P("C14.1")])
T("+1V2", "B", 0.2, [P("FB2.1"), (58.84, 59.2), (59.5, 59.86), core])
# VCCIO_0: pin 33 -> via (+3V3 plane) -> C15
T("+3V3", "F", 0.2, [P("U2.33"), io]); VIA("+3V3", *io, d=0.45, h=0.25)
T("+3V3", "B", 0.3, [io, P("C15.1")])
# GND: the four inward GND pads strapped on the back, tied into the EP's second via column
T("GND", "B", 0.4, [P("C20.2"), P("C19.2"), P("C14.2"), P("C15.2")])
for c, v in (("C19.2", (64.093, 59.492)), ("C14.2", (64.093, 60.693)), ("C15.2", (64.093, 61.892))):
    T("GND", "B", 0.4, [P(c), v])
