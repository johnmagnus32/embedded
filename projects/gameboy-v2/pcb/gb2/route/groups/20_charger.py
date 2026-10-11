# Charger + USB-C signals. Status lines leave U9 north, run west on the back between J7's shell-tab rows (channel
# "N": BAT sense tap on top, then /PGOOD, then /CHG, so each peels off south without a crossing) and come up beside
# the MCU's top pins: /PGOOD -> PC2 (pin 10), /CHG -> PC1 (pin 9), BAT -> R26 (battery-sense divider, back).
# Programming resistors sit right on their pins. TS (NTC, R20 on the back at the battery cutout) goes straight down
# the front between the BAT trunk and NOR and drops to the back just above the bottom-edge channel E2.

# ---- USB-C CC pull-downs: straight down from the receptacle
T("USBC_CC1", "F", 0.2, [P("J7.A5"), P("R15.1")])
T("USBC_CC2", "F", 0.2, [P("J7.B5"), P("R16.1")])

# ---- programming pins
T("CHG_EN2", "F", 0.2, [P("R17.1"), (108.25, 37.3), P("U9.5")])
T("CHG_ISET", "F", 0.2, [P("U9.16"), (108.25, 41.5), (108.7, 41.95), P("R18.1")])
T("CHG_TMR", "F", 0.2, [P("U9.14"), (107.25, 41.6), (107.6, 41.95), P("R21.1")])

# ---- /PGOOD: U9.7 up to its pull-up R23, west under R22's body, via, west along the back (y 35.4), down at x 87.2
pg = "CHG_PGOOD"; pv, pm = (102.5, 35.92), (87.2, 41.3)
T(pg, "F", 0.2, [P("U9.7"), (107.25, 37.2), P("R23.1")])
T(pg, "F", 0.15, [P("R23.1"), (106.4, 35.92), pv]); VIA(pg, *pv, d=0.45, h=0.25)
T(pg, "B", 0.15, [pv, (102.0, 35.4), (88.2, 35.4), (87.2, 36.4), pm]); VIA(pg, *pm, d=0.45, h=0.25)
T(pg, "F", 0.15, [pm, (87.25, 41.35), P("U1.10")])

# ---- /CHG: U9.9 up to R22, west, via, west along the back (y 35.9), down at x 87.9
st = "CHG_STAT"; sv, sm = (101.7, 36.8), (87.9, 42.2)
T(st, "F", 0.2, [P("U9.9"), (105.75, 38.75), (105.55, 38.55), (105.55, 37.0), P("R22.1")])
T(st, "F", 0.15, [P("R22.1"), (102.0, 36.67), sv]); VIA(st, *sv, d=0.45, h=0.25)
T(st, "B", 0.15, [sv, (100.8, 35.9), (88.6, 35.9), (87.9, 36.6), sm]); VIA(st, *sm, d=0.45, h=0.25)
T(st, "F", 0.15, [sm, (87.75, 42.6), P("U1.9")])

# ---- BAT_SENSE: divider node (back) up through a via beside C45 to PC0 (pin 8)
bv = (88.25, 47.75)
T("BAT_SENSE", "B", 0.2, [P("R26.2"), P("R27.1")])
T("BAT_SENSE", "B", 0.2, [P("R27.1"), P("C45.1")])
T("BAT_SENSE", "B", 0.2, [P("C45.1"), bv]); VIA("BAT_SENSE", *bv, d=0.45, h=0.25)
T("BAT_SENSE", "F", 0.15, [bv, P("U1.8")])

# ---- TS: U9.1 east, down the front at x 109.75 / 110.3 (clear of R18, the BAT trunk, C24, U7), via above E2, R20
ts = "CHG_TS"; tv = (110.3, 61.6)
T(ts, "F", 0.15, [P("U9.1"), (109.55, 40.8), (109.75, 41.0), (109.75, 45.0), (110.3, 45.55), tv]); VIA(ts, *tv, d=0.45, h=0.25)
T(ts, "B", 0.15, [tv, (112.6, 63.9), P("R20.1")])
