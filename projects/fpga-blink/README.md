# FPGA Blink — iCE40UP5K on iCEBreaker

Blinks the onboard RGB LED using the internal 12MHz oscillator.

## Prerequisites

```bash
sudo apt install iverilog gtkwave yosys nextpnr-ice40 fpga-icestorm
```

## Simulate (no hardware needed)

```bash
make sim        # runs testbench, prints PASS/FAIL
make wave       # opens waveform viewer
```

## Synthesize (verify it fits the chip)

```bash
make synth      # produces blink.bin, reports LUT/RAM utilization
```

## Program the iCEBreaker

```bash
make prog       # uploads bitstream via USB
```

The RGB LED will cycle through colors at ~1-2Hz.
