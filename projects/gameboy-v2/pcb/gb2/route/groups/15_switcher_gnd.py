# Top-layer GND copper under each switching stage, so every hot loop (input cap -> switch -> GND -> cap) also closes
# on F.Cu right at the parts, not only through the In1 plane 0.2 mm down. KiCad fills around every other net; the
# caps' GND pads connect solid (no thermal spokes) and the pour bonds to their GND / thermal vias. Each region stops
# short of the reserved channels and of the USB receptacle.
ZONE("GND", "F", [(60.6, 33.1), (89.4, 33.1), (89.4, 44.2), (73.6, 44.2), (73.6, 43.4), (60.6, 43.4)],
     "GND pour: LCD boost (U5) + buck-boost (U10)")
ZONE("GND", "F", [(100.6, 33.1), (114.0, 33.1), (114.0, 44.6), (100.6, 44.6)], "GND pour: charger (U9)")
ZONE("GND", "F", [(145.0, 49.2), (155.4, 49.2), (155.4, 58.2), (145.0, 58.2)], "GND pour: amp (U6)")
