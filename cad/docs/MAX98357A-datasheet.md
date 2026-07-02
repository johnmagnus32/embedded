## General Description............................................................................

#### The MAX98357A/MAX98357B is an easy-to-use, low-cost,

#### digital pulse-code modulation (PCM) input Class D ampli-

#### fier that provides industry-leading Class AB audio perfor-

#### mance with Class D efficiency. The digital audio interface

#### automatically recognizes up to 35 different PCM and TDM

#### clocking schemes which eliminates the need for I^2 C pro-

#### gramming. Operation is further simplified by eliminating

#### the need for an external MCLK signal that is typically used

#### for PCM communication. Simply supply power, LRCLK,

#### BCLK, and digital audio to generate audio! Furthermore,

#### a novel pinout allows customers to use the cost-effective

#### WLP package with no need for expensive vias (refer

#### to Application Note 6643: Optimize Cost, Size, and

#### Performance with MAX98357 WLP for more information).

#### The digital audio interface is highly flexible with the

#### MAX98357A supporting I^2 S data and the MAX98357B

#### supporting left-justified data. Both ICs support 8 channel

#### time division multiplexed (TDM) data. The digital audio

#### interface accepts specified sample rates between 8kHz

#### and 96kHz for all supported data formats. The ICs can

#### be configured to produce a left channel, right channel, or

#### (left/2 + right/2) output from the stereo input data. The ICs

#### operate using 16/24/32-bit data for I^2 S and left-justified

#### modes as well as 16-bit or 32-bit data using TDM mode.

#### The ICs eliminate the need for the external MCLK signal

#### that is typically used for PCM communication. This reduc-

#### es EMI and possible board coupling issues in addition to

#### reducing the size and pin count of the ICs.

#### The ICs also feature a very high wideband jitter toler-

#### ance (12ns typ) on BCLK and LRCLK to provide robust

#### operation.

#### Active emissions-limiting, edge-rate limiting, and over-

#### shoot control circuitry greatly reduce EMI. A filterless

#### spread-spectrum modulation scheme eliminates the need

#### for output filtering found in traditional Class D devices and

#### reduces the component count of the solution.

#### The ICs are available in 9-pin WLP (1.345mm x 1.435mm

#### x 0.64mm) and 16-pin TQFN (3mm x 3mm x 0.75mm)

#### packages and are specified over the -40°C to +85°C tem-

#### perature range.

## Features.....................................................................................

#### ● Single-Supply Operation (2.5V to 5.5V)

#### ● 3.2W Output Power into 4Ω at 5V

#### ● 2.4mA Quiescent Current

#### ● 92% Efficiency (RL = 8Ω, POUT = 1W)

#### ● 22.8μVRMS Output Noise (AV = 15dB)

#### ● Low 0.013% THD+N at 1kHz

#### ● No MCLK Required

#### ● Sample Rates of 8kHz to 96kHz

#### ● Supports Left, Right, or (Left/2 + Right/2) Output

#### ● Sophisticated Edge Rate Control Enables

#### Filterless Class D Outputs

#### ● 77dB PSRR at 1kHz

#### ● Low RF Susceptibility Rejects TDMA

#### Noise from GSM Radios

#### ● Extensive Click-and-Pop Reduction Circuitry

#### ● Robust Short-Circuit and Thermal Protection

#### ● Available in Space-Saving Packages:

#### 1.345mm x 1.435mm WLP (0.4mm Pitch)

#### and 3mm x 3mm TQFN

#### ● Solution Size with Single Bypass Capacitor is

#### 4.32mm^2

## Applications

#### ● Single Li-ion Cell/5V Devices

#### ● Smart Speakers

#### ● Notebook Computers

#### ● IoT Devices

#### ● Gaming Devices (Audio and Haptics)

#### ● Smartphones

#### ● Tablets

#### ● Cameras

```
19-6779; Rev 16; 2/
```
**_Ordering Information appears at end of data sheet._**

**_Functional Diagram appears at end of data sheet._**

```
Click here to ask an associate for production status of specific part numbers.
```
## Simplified Block Diagram........................................................................

```
DAC
```
```
CLASS D
OUTPUT
STAGE
```
```
DIGITAL
AUDIO
INTERFACE
```
```
PCM
INPUT
```
```
GAIN
CONTROL
```
```
SHUTDOWN
AND
CHANNEL
SELECT
```
```
MAX98357A
MAX98357B
```
## Tiny, Low-Cost, PCM Class D Amplifier with

## Class AB Performance

# MAX98357A/

# MAX98357B

One Analog Way, Wilmington, MA 01887 U.S.A. | Tel: 800.262.5643 | © 2026 Analog Devices, Inc. All rights reserved.

© 2026 Analog Devices, Inc. All rights reserved. Trademarks and registered trademarks are the property of their respective owners.

##### EVALUATION KIT AVAILABLE


## TABLE OF CONTENTS

**MAX98357B**


LIST OF FIGURES

**MAX98357B**

- General Description............................................................................
- Features.....................................................................................
- Applications
- Simplified Block Diagram........................................................................
- Absolute Maximum Ratings......................................................................
- Package Thermal Characteristics
- Electrical Characteristics
- Typical Operating Characteristics
- Pin Configurations
- Pin Description...............................................................................
- Detailed Description...........................................................................
   - Digital Audio Interface Modes..................................................................
      - MCLK Elimination
      - BCLK Jitter Tolerance
      - BCLK Polarity............................................................................
      - LRCLK Polarity
      - Standby Mode
   - DAC Digital Filters...........................................................................
   - S D_M O D E and Shutdown Operation.............................................................
      - Startup
      - I^2 S and Left Justified Mode.................................................................
      - TDM Mode..............................................................................
   - Class D Speaker Amplifier
      - Ultra-Low EMI Filterless Output Stage
      - Speaker Current Limit
      - Gain Selection
      - Click-and-Pop Suppression.................................................................
- Applications Information........................................................................
   - Filterless Class D Operation...................................................................
   - Power-Supply Input..........................................................................
   - Layout and Grounding........................................................................
- Functional Diagram
   - WLP Applications Information..................................................................
- Ordering Information
- Package Information
- Revision History..............................................................................
- Figure 1. I^2 S Audio Interface Timing Diagram (MAX98357A)............................................ LIST OF TABLES
- Figure 2. Left-Justified Audio Interface Timing Diagram (MAX98357B)....................................
- Figure 3. TDM Audio Interface Timing Diagram
- Figure 4. SD_MODE Resistor Connected Using Open-Drain Driver
- Figure 5. SD_MODE Resistor Connected Using Push-Pull Driver........................................
- Figure 6. Required startup sequence when using BCLK = 256kHz
- Figure 7. MAX98357A I^2 S Digital Audio Interface Timing, 16-Bit Resolution...............................
- Figure 8. MAX98357A I^2 S Digital Audio Interface Timing, 32-Bit Resolution...............................
- Figure 9. MAX98357B Left-Justified Digital Audio Interface Timing, 16-Bit Resolution.......................
- Figure 10. MAX98357B Left-Justified Digital Audio Interface Timing, 32-Bit Resolution......................
- Figure 11. MAX98357A TDM 16-Bit DAI Timing
- Figure 12. MAX98357A TDM 32-Bit DAI Timing.....................................................
- Figure 13. MAX98357B TDM 16-Bit DAI Timing.....................................................
- Figure 14. MAX98357B TDM 32-Bit DAI Timing.....................................................
- Figure 15. EMI with 12in of Speaker Cable and No Output Filtering......................................
- Figure 16. Left-Channel PCM Operation with 6dB Gain...............................................
- Figure 17. Left-Channel PCM Operation with 12dB Gain
- Figure 18. Right-Channel PCM Operation with 6dB Gain..............................................
- Figure 19. (Left/2 + Right/2) PCM Operation with 6dB Gain............................................
- Figure 20. Stereo PCM Operation Using Two ICs....................................................
- Figure 21. Channel TDM Operation (Gain Fixed at 12dB)..............................................
- Figure 22. WLP Pin Connect for set 12dB Gain Without Via............................................
- Figure 23. Example Layout Configured for Left-Channel Audio and Gain of 12dB...........................
- Figure 24. MAX98357A/MAX98357B WLP Ball Dimensions
- Table 1. RMS Jitter Tolerance
- Table 2. BCLK Polarity.........................................................................
- Table 3. LRCLK Polarity........................................................................
- Table 4. Digital Filter Settings
- Table 5. SD_MODE Control
- Table 6. Examples of SD_MODE Pullup Resistor Values...............................................
- Table 7. TDM Mode Channel Selection............................................................
- Table 8. Gain Selection


**(Note 1)**

VDD, LRCLK, BCLK, and DIN to GND ....................-0.3V to +6V
All Other Pins to GND ..............................-0.3V to (VDD + 0.3V)
Continuous Current In/Out of VDD/GND/OUT_..................±1.6A
Continuous Input Current (all other pins) .........................±20mA
Duration of OUT_ Short Circuit to GND or VDD..... ..Continuous
Duration of OUTP Short to OUTN .............................Continuous

```
Continuous Power Dissipation (TA = +70°C)
WLP (derate 13.7mW/°C above +70°C) ....................1096mW
TQFN (derate 20.8mW/°C above +70°C)..................1666mW
Junction Temperature ......................................................+150°C
Operating Temperature Range ...........................-40°C to +85°C
Storage Temperature Range ............................-65°C to +150°C
Soldering Temperature (reflow) .......................................+260°C
Lead Temperature (soldering, 10s, TQFN) .....................+300°C
```
##### WLP

```
Junction-to-Ambient Thermal Resistance (θJA) ..........73°C/W
Junction-to-Case Thermal Resistance (θJC) ...............50°C/W
```
##### TQFN

```
Junction-to-Ambient Thermal Resistance (θJA) ..........48°C/W
Junction-to-Case Thermal Resistance (θJC) .................7°C/W
```
(VDD = 5V, VGND = 0V, GAIN_SLOT = VDD. BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between OUTP
and OUTN, ZSPK = ∞, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.) (Note 2)

```
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
Supply Voltage Range VDD Guaranteed by PSSR test 2.5 5.5 V
Undervoltage Lockout UVLO 1.5 1.8 2.3 V
```
```
Quiescent Current IDD
```
##### TA = +25°C 2.75 3.

```
mA
TA = +25°C, VDD = 3.7V 2.4 2.
Shutdown Current ISHDN SD_MODE = 0V, TA = +25°C 0.6 2 μA
Standby Current ISTNDBY SD_MODE = 1.8V, no BCLK, TA = +25°C 340 400 μA
```
```
Turn-On Time tON 7 7.5 ms
```
```
Output Offset Voltage VOS TA = +25°C, gain = 15dB ±0.3 ±2.5 mV
```
```
Click-and-Pop Level KCP
```
```
Peak voltage, TA =
+25°C, A-weighted,
32 samples per
second (Note 3)
```
```
Into shutdown -
dBV
Out of shutdown -
```
```
Power-Supply Rejection Ratio PSRR
```
```
VDD = 2.5V to 5.5V, TA = +25°C 60 75
```
```
TA = +25°C dB
(Notes 3, 4)
```
```
f = 217Hz,
200mVP-P ripple
```
##### 77

```
f = 10kHz,
200mVP-P ripple^60
```
## Absolute Maximum Ratings......................................................................

_Stresses beyond those listed under “Absolute Maximum Ratings” may cause permanent damage to the device. These are stress ratings only, and functional operation of the device at these
or any other conditions beyond those indicated in the operational sections of the specifications is not implied. Exposure to absolute maximum rating conditions for extended periods may affect
device reliability._

## Electrical Characteristics

**Note 1:** Package thermal resistances were obtained using the method described in JEDEC specification JESD51-7, using a four-layer
board. For detailed information on package thermal considerations, refer to **_[http://www.analog.com/thermal-tutorial](http://www.analog.com/thermal-tutorial)_**.

## Package Thermal Characteristics

**MAX98357B**
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = VDD. BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between OUTP
and OUTN, ZSPK = ∞, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.) (Note 2)

```
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
```
```
Output Power (Note 3) POUT
```
##### THD+N 10%,

```
gain = 12dB
```
```
ZSPK = 4Ω + 33μH 3.
```
##### W

```
ZSPK = 8Ω + 68μH 1.
ZSPK = 8Ω + 68μH,
VDD = 3.7V 0.
```
##### THD+N = 1%,

```
gain = 12dB
```
```
ZSPK = 4Ω + 33μH 2.
ZSPK = 8Ω + 68μH 1.
ZSPK = 8Ω + 68μH,
VDD = 3.7V 0.
```
```
Total Harmonic Distortion +
Noise THD+N
```
```
f = 1kHz, POUT = 1W, TA = +25°C,
ZSPK = 4Ω + 33μH, WLP
```
##### 0.02 0.

##### %

```
f = 1kHz, POUT = 1W, TA = +25°C,
ZSPK = 4Ω + 33μH, TQFN 0.
f = 1kHz, POUT = 0.5W, TA = +25°C,
ZSPK = 8Ω + 68μH
```
##### 0.

```
Dynamic Range DR
```
```
A-weighted, ZSPK = 8Ω + 33μH,
VRMS = 3.40V, 24- or 32-bit data 103.5 dB
Output Noise VN A-weighted, 24- or 32-bit data (Note 4) 22.8 μVRMS
```
```
Gain (Relative to a 2.1dBV
Reference Level) AV
```
```
GAIN_SLOT = GND through 100kΩ 14.4 15 15.
```
```
dB
```
##### GAIN_SLOT = GND 11.4 12 12.

```
GAIN_SLOT = unconnected 8.4 9 9.
GAIN_SLOT = VDD 5.4 6 6.
GAIN_SLOT = VDD through 100kΩ 2.4 3 3.
Current Limit ILIM 2.8 A
```
```
Efficiency ε ZSPK = 8Ω + 68μH, THD+N = 10%,
f = 1kHz, gain = 12dB
```
##### 92 %

```
DAC Gain Error 1 %
Frequency Response -0.2 +0.2 dB
Class D Switching Frequency fOSC 300 kHz
Spread-Spectrum Bandwidth ±20 kHz
DAC DIGITAL FILTERS
VOICE MODE IIR LOWPASS FILTER (LRCLK < 30kHz)
```
```
Passband Cutoff fPLP
```
```
Ripple limit cutoff 0.^
x fS
Hz
-3dB cutoff
```
##### 0.

```
x fS
```
```
Stopband Cutoff fSLP 0.
x fS
```
```
Hz
```
```
Stopband Attenuation f > fSLP 75 dB
```
## Electrical Characteristics (continued)

MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = VDD. BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between OUTP
and OUTN, ZSPK = ∞, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.) (Note 2)

```
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
AUDIO MODE FIR LOWPASS FILTER (30kHz < LRCLK < 50kHz)
```
```
Passband Cutoff fPLP
```
```
Ripple limit cutoff
```
##### 0.

```
x fS
```
```
-3dB cutoff Hz
```
##### 0.

```
x fS
```
```
-6.02dB cutoff 0.^
x fS
```
```
Stopband Cutoff fSLP
```
##### 0.

```
x fS Hz
Stopband Attenuation f > fSLP 60 dB
AUDIO MODE FIR LOWPASS FILTER (LRCLK > 50kHz)
```
```
Passband Cutoff fPLP
```
```
Ripple limit cutoff
```
##### 0.

```
x fS
Hz
-3dB cutoff 0.^
x fS
```
```
Stopband Cutoff fSLP
```
##### 0.

```
x fS Hz
Stopband Attenuation f < fSLP 60 dB
DIGITAL AUDIO INTERFACE
LRCLK Range 1 fS1 7.6 8 8.
```
```
kHz
```
```
LRCLK Range 2 fS2 15.2 16 16.
LRCLK Range 3 fS3 30.4 48 50.
LRCLK Range 4 fS4 83.8 96 100.
```
```
Resolution
```
```
I^2 S/left justified mode 16/24/
Bits
TDM mode 16/
BCLK Frequency Range fBCLKH BCLK must be 32, 48, or 64X of LRCLK 0.2432 25.804 MHz
BCLK High Time tBCLKH 15 ns
BCLK Low Time tBCLKL 15 ns
Maximum Low Frequency
BCLK and LRCLK Jitter
```
```
RMS jitter below 40kHz 0.
ns
Maximum High Frequency
BCLK and LRCLK Jitter RMS jitter above 40kHz^12
Input High Voltage VIH Digital audio inputs 1.3 V
```
## Electrical Characteristics (continued)

MAX98357B
Class AB Performance


**Note 2:** 100% production tested at TA = +25°C. Specifications over temperature limits are guaranteed by design.
**Note 3:** Class D amplifier testing performed with a resistive load in series with an inductor to simulate an actual speaker load. For
RL = 8Ω, LL = 68μH. For RL = 4Ω, LL = 33μH.
**Note 4:** Digital silence used for input signal.
**Note 5:** Dynamic range measured using the EIAJ method. -60dBFS 1kHz output signal, A-weighted, and normalized to 0dBFS.
f = 20Hz to 20kHz.

(VDD = 5V, VGND = 0V, GAIN_SLOT = VDD. BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between OUTP
and OUTN, ZSPK = ∞, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.) (Note 2)

```
PARAMETER SYMBOL CONDITIONS MIN TYP MAX UNITS
Input Low Voltage VIL Digital audio inputs 0.6 V
Input Leakage Current IIH, IIL VIN = 0V, VDD = 5.5V, TA = +25°C -1 +1 μA
Input Capacitance CIN 3 pF
DIN to BCLK Setup Time tSETUP 10 ns
LRCLK to BCLK Setup Time tSYNCSET 10 ns
DIN to BCLK Hold Time tHOLD 10 ns
LRCLK to BCLK Hold Time tSYNCHOLD 10 ns
SD_MODE COMPARATOR TRIP POINTS
B
See SD_MODE and shutdown operation
for details
```
##### 0.08 0.16 0.

##### B1 0.65 0.77 0.825 V

##### B2 1.245 1.4 1.

```
SD_MODE Pulldown Resistor RPD 92 100 108 kΩ
GAIN COMPARATOR TRIP POINTS
```
##### V_GAIN_

##### SLOT

```
AV = 3dB gain 0.65 x
VDD
```
```
0.85 x
VDD
```
##### V

```
AV = 6dB gain
```
```
0.9 x
VDD VDD
```
```
AV = 9dB gain 0.4 x
VDD
```
```
0.6 x
VDD
```
```
AV = 12dB gain 0 0.1 x
VDD
```
```
AV = 15dB gain
```
```
0.15 x
VDD
```
```
0.35 x
VDD
```
## Electrical Characteristics (continued)

**MAX98357B**
Class AB Performance


_Figure 1. I_^2 _S Audio Interface Timing Diagram (MAX98357A)_

## Figure 3. TDM Audio Interface Timing Diagram

## Figure 2. Left-Justified Audio Interface Timing Diagram (MAX98357B)....................................

```
LRCLK (INPUT)
```
```
BCLK (INPUT)
```
```
DIN (INPUT) LEFT MSB
```
```
tSETUP tHOLD
```
```
tBCLKH
tBCLKL
```
```
tBCLK
```
```
tSYNCSET
```
```
RIGHT MSB
```
```
tSYNCHOLD
```
```
VIH
VIL
```
```
VIH
VIL
```
```
VIH
```
```
VIH
VIL VIL
```
```
VIH
```
```
VIL
```
```
LRCLK (INPUT)
```
```
BCLK (INPUT)
```
```
DIN (INPUT)
```
```
tSETUP tHOLD
```
```
tSYNCHOLD
```
```
MSB
```
```
tBCLKHtBCLKL
```
```
tBCLK
```
```
tSYNCSET
```
```
LRCLK (input)
```
```
BCLK (input)
```
```
DIN (input)
```
```
tSETUPtHOLD
```
```
tSYNCHOLD
```
```
MSB
```
```
tBCLK tBCLKLtBCLKH
```
```
tSYNCSET
```
```
MAX 98357 A MAX 98357 B
```
```
VIL
```
```
VIH
```
```
VILVIH VILVIH
```
```
VILVIH
```
```
VIHVIL
```
```
VIH
VIL
```
```
VIL VIH
```
```
VILVIH
```
```
LRCLK (INPUT)
```
```
BCLK (INPUT)
```
```
DIN (INPUT) LEFT MSB
```
```
tSETUP tHOLD
```
```
tBCLKH
tBCLKL
```
```
tBCLK
```
```
tSYNCSET tSYNCHOLD
```
```
VIH
VIL
```
```
VIH
VIL
```
```
VIH
```
```
VIH
VIL VIL
```
```
VIH
```
```
VIL RIGHT MSB
```
**MAX98357B**
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics

```
0.
```
```
0.
```
```
1.
```
```
1.
```
```
2.
```
```
2.
```
```
3.
```
```
3.
```
```
4.
```
```
2.5 3.0 3.5 4.0 4.5 5.0 5.
```
```
QUIECENT CURRENT (mA)
```
```
SUPPLY VOLTAGE (V)
```
```
QUIESCENT CURRENT vs.
SUPPLY VOLTAGE
VDDIO= 1.8V
```
```
toc
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
0.
```
```
1.
```
```
2.5 3.0 3.5 4.0 4.5 5.0 5.
```
```
SHUTDOWN CURRENT (μA)
```
```
SUPPLY VOLTAGE (V)
```
```
SHUTDOWN CURRENT vs. SUPPLY VOLTAGE toc
VDDIO= 1.8V
```
MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics (continued)

```
0.
```
```
0.
```
```
1.
```
```
1.
```
```
2.
```
```
2.
```
```
1 10 100
```
```
OUTPUT POWER (W)
```
```
LOAD RESISTANCE
```
```
OUTPUT POWER vs. LOAD RESISTANCE
VDD= 3.7V
```
```
10%THD+N
```
```
1% THD+N
```
```
toc
```
```
0.
```
```
0.
```
```
1.
```
```
1.
```
```
2.
```
```
2.
```
```
3.
```
```
3.
```
```
1 10 100
```
```
OUTPUT POWER (W)
```
```
LOAD RESISTANCE
```
```
OUTPUT POWER vs. LOAD RESISTANCE
VDD= 4.2V
```
```
10%THD+N
```
```
1% THD+N
```
```
toc
```
```
0.
```
```
0.
```
```
1.
```
```
1.
```
```
2.
```
```
2.
```
```
3.
```
```
3.
```
```
4.
```
```
4.
```
```
1 10 100
```
```
OUTPUT POWER (W)
```
```
LOAD RESISTANCE
```
```
OUTPUT POWER vs. LOAD RESISTANCE
VDD= 5V
```
```
10%THD+N
```
```
1%THD+N
```
```
toc
```
MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics (continued)

```
-3.
```
```
-2.
```
```
-2.
```
```
-1.
```
```
-1.
```
```
-0.
```
```
0.
```
```
0.
```
```
1.
```
```
1 10 100 1000 10000 100000
```
```
NORMALIZED GAIN (d B)
```
```
FREQUENCY (Hz)
```
```
NORMALIZED GAIN vs. FREQUENCY toc
```
MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics (continued)

```
0
```
```
10
```
```
20
```
```
30
```
```
40
```
```
50
```
```
60
```
```
70
```
```
80
```
```
90
```
```
100
```
```
10 100 1000 10000 100000
```
```
PSRR (dB)
```
```
FREQUENCY (Hz)
```
```
POWER-SUPPLY REJECTION RATIO
vs. FREQUENCY toc
```
```
0
```
```
10
```
```
20
```
```
30
```
```
40
```
```
50
```
```
60
```
```
70
```
```
80
```
```
90
```
```
100
```
```
2.5 3.0 3.5 4.0 4.5 5.0 5.
```
```
PSRR (dB)
```
```
SUPPLY VOLTAGE (V)
```
```
POWER-SUPPLY REJECTION RATIO
vs. SUPPLY VOLTAGE
```
```
fS= 1kHz
```
```
toc28 TURN-ON RESPONSE
OUTPUT
1V/div
```
```
SD_MODE
1V/div
```
```
toc
```
```
2ms/div
```
```
TURN-OFF RESPONSE
```
```
OUTPUT
1V/div
```
```
SD_MODE
1V/div
```
```
toc
```
```
1ms/div
```
```
BCLK
2V/div
LRCLK
2V/div
```
```
OUTPUT
1V/div
```
```
toc30a
```
```
500μs/div
```
```
TURN-OFF RESPONSE
(STANDBY MODE)
```
```
BCLK
2V/div
LRCLK
2V/div
```
```
OUTPUT
1V/div
```
```
toc30b
```
```
2ms/div
```
```
TURN-ON RESPONSE
(STANDBY MODE)
```
MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics (continued)

MAX98357B
Class AB Performance


(VDD = 5V, VGND = 0V, GAIN_SLOT = GND (+12dB). BCLK = 3.072MHz, LRCLK = 48kHz, speaker loads (ZSPK) connected between
OUTP and OUTN, TA = TMIN to TMAX, unless otherwise noted. Typical values are at TA = +25°C.)

## Typical Operating Characteristics (continued)

MAX98357B
Class AB Performance


##### PIN

##### NAME FUNCTION

##### WLP TQFN

##### A1 4 SD_MODE

```
Shutdown and Channel Select. Pull SD_MODE low to place the device in shutdown. In I^2 S
or LJ mode, SD_MODE selects the data channel (Table 5). In TDM mode, SD_MODE and
GAIN_SLOT are both used for channel selection (Table 7).
A2 7, 8 VDD Power-Supply Input
A3 9 OUTP Positive Speaker Amplifier Output
B1 1 DIN Digital Input Signal
```
##### B2 2 GAIN_

##### SLOT

```
Gain and Channel Selection. In I^2 S and LJ mode determines amplifier output gain (Table 8)
In TDM mode, used for channel selection with SD_MODE (Table 7). In TDM mode, gain is
fixed at 12dB.
B3 10 OUTN Negative Speaker Amplifier Output
C1 16 BCLK Bit Clock Input
C2 3, 11, 15 GND Ground
C3 14 LRCLK Frame Clock. Left/right clock for I^2 S and LJ mode. Sync clock for TDM mode.
```
```
— 5, 6,^
12, 13
```
```
N.C. No Connection
```
```
— — EP Exposed Pad. The exposed pad is not internally connected. Connect the exposed page to a
solid ground plane for thermal dissipation.
```
## Pin Description...............................................................................

## Pin Configurations

```
WLP
```
```
TOP VIEW
BUMP SIDE DOWN
```
```
BCLK GND LRCLK
```
```
DIN GAIN_SLOT OUTN
```
```
SD_MODE VDD OUTP
```
```
MAX98357A
MAX98357B
+
```
```
A
```
```
B
```
```
C1 C2 C
```
```
B2 B
```
```
A2 A
```
```
15
```
```
16
```
```
14
```
```
13
```
```
5
```
```
6
```
```
7
```
```
GND
SD_MODE
```
```
8
```
```
DIN
```
```
N.C. OUTNOUTP
```
```
1 3
```
```
LRCLK
```
```
4
```
```
12 10 9
```
```
GND
```
```
BCLK
```
```
VDD
```
```
VDD
```
```
N.C.
```
```
N.C.
```
```
GAIN_SLOT
```
```
GND
```
```
2
```
```
11
```
```
N.C.
```
```
TQFN
```
```
MAX98357A
MAX98357B
```
```
TOP VIEW
```
```
+
```
**MAX98357B**
Class AB Performance


## Detailed Description...........................................................................

#### The MAX98357A/MAX98357B are digital PCM input

#### Class D power amplifiers. The MAX98357A accepts stan-

#### dard I^2 S data through DIN, BCLK, and LRCLK while the

#### MAX98357B accepts left-justified data through the same

#### inputs. Both versions also accept 16-bit or 32-bit TDM

#### data with up to eight slots. The digital audio interface

#### eliminates the need for an external MCLK signal that is

#### typically required for I^2 S data transmission.

#### SD_MODE selects which data word is output by the

#### amplifier and is used to put the ICs into shutdown. These

#### devices offer five gain settings in I^2 S/left-justified mode

#### and a fixed 12dB gain in TDM mode. Channel selection in

#### TDM mode is set with the combination of SD_MODE and

#### GAIN_SLOT (Table 7).

#### The MAX98357A/MAX98357B DAI includes a DC blocker

#### with a -3dB cutoff at 3.7Hz.

#### The MAX98357A/MAX98357B feature low-quiescent cur-

#### rent, comprehensive click-and-pop suppression, and

#### excellent RF immunity. The ICs offer Class AB audio

#### performance with Class D efficiency in a minimal board-

#### space solution. The Class D amplifier features spread-

#### spectrum modulation with edge-rate and overshoot con-

#### trol circuitry that offers significant improvements in switch-

#### mode amplifier radiated emissions. The amplifier features

#### click-and-pop suppression that reduces audible transients

#### on startup and shutdown. The amplifier includes thermal-

#### overload and short-circuit protection.

### Digital Audio Interface Modes..................................................................

#### The input stage of the digital audio interface is highly flexi-

#### ble, supporting 8kHz–96kHz sampling rates with 16/24/32-

#### bit resolution for I^2 S/left justified data as well as up to a

#### 8-slot, 16-bit or 32-bit time division multiplexed (TDM)

#### format. When LRCLK has a 50% duty cycle the data

#### format is determined by the part number selection

#### (MAX98357A/MAX98357B). When a frame sync pulse

#### is used for the LRCLK the data format is automatically

#### configured in TDM mode. The frame sync pulse indicates

#### the beginning of the first time slot.

#### MCLK Elimination

#### The ICs eliminate the need for the external MCLK

#### signal that is typically used for PCM communication.

#### This reduces EMI and possible board coupling issues in

#### addition to reducing the size and pin-count of the ICs.

#### BCLK Jitter Tolerance

#### The ICs feature a BCLK jitter tolerance of 0.5ns for RMS

#### jitter below 40kHz and 12ns for wideband RMS jitter while

#### maintaining a dynamic range greater than 98dB (Table 1).

#### BCLK Polarity............................................................................

#### When operating in I^2 S/left-justified mode, incoming serial

#### data is always clocked-in on the rising edge of BCLK.

#### In TDM mode, the MAX98357A clocks-in serial data on

#### the rising edge of BCLK while the MAX98357B clocks in

#### serial data on the falling edge of BCLK (Table 2).

#### LRCLK Polarity

#### LRCLK specifies whether left-channel data or right-

#### channel data is currently being read by the digital audio

#### interface. The MAX98357A indicates the left channel

#### word when LRCLK is low, and the MAX98357B indicates

#### the left channel word when LRCLK is high (Table 3).

#### LRCLK ONLY supports 8kHz, 16kHz, 32kHz, 44.1kHz,

#### 48kHz, 88.2kHz and 96kHz frequencies. LRCLK clocks

#### at 11.025kHz, 12kHz, 22.05kHz and 24kHz are NOT sup-

#### ported. Do not remove LRCLK while BCLK is present.

#### Removing LRCLK while BCLK is present can cause unex-

#### pected output behavior including a large DC output voltage.

#### Standby Mode

#### The ICs automatically enter standby mode when BCLK

#### is removed. If BCLK stops toggling, the ICs automatically

## Table 1. RMS Jitter Tolerance

## Table 2. BCLK Polarity.........................................................................

## Table 3. LRCLK Polarity........................................................................

```
FREQUENCY RMS JITTER TOLERANCE (ns)
< 40kHz 0.
40kHz–BCLK 12
```
##### MODE PART NUMBER BCLK POLARITY

```
I^2 S MAX98357A Rising edge
Left-justified MAX98357B Rising edge
```
```
TDM
```
```
MAX98357A Rising edge
MAX98357B Falling edge
```
##### PART NUMBER LRCLK POLARITY (LEFT CHANNEL)

```
MAX98357A Low
MAX98357B High
```
**MAX98357B**
Class AB Performance


#### enter standby mode. In standby mode, the Class D speak-

#### er is turned off and the outputs go into a high-impedance

#### state, ensuring that unwanted current is not transferred to

#### the load during this condition. Standby mode has reduced

#### power consumption from normal operation (340μA), but

#### does not reach as low as full shutdown (0.6μA). Standby

#### mode can be used to reduce power consumption when no

#### GPIO us available to pull SD_MODE low.

### DAC Digital Filters...........................................................................

#### The DAC features a digital lowpass filter that is auto-

#### matically configured for voice playback or music playback

#### based on the sample rate that is used. This filter elimi-

#### nates the effect of aliasing and any other high-frequency

#### noise that might otherwise be present. Table 4 shows the

#### digital filter settings that are automatically selected.

### S D_M O D E and Shutdown Operation.............................................................

#### The ICs feature a low-power shutdown mode, drawing

#### less than 0.6μA (typ) of supply current. During shutdown,

#### all internal blocks are turned off, including setting the

#### output stage to a high-impedance state. Drive SD_MODE

#### low to put the ICs into shutdown.

#### The state of SD_MODE determines the audio channel

#### that is sent to the amplifier output (Table 5).

#### Drive SD_MODE high to select the left word of the stereo

#### input data. Drive SD_MODE high through a sufficiently

#### small resistor to select the right word of the stereo input

#### data. Drive SD_MODE high through a sufficiently large

#### resistor to select both the left and right words of the

#### stereo input data (left/2 + right/2). RLARGE and RSMALL

#### are determined by the VDDIO voltage (logic voltage from

#### control interface) that is driving SD_MODE according to

#### the following two equations:

#### RSMALL (kΩ) = 94.0 x VDDIO - 100

#### RLARGE (kΩ) = 222.2 x VDDIO - 100

#### When the devices are configured in left-channel mode

#### (SD_MODE is directly driven to logic-high by the con-

#### trol interface), take care to avoid violating the Absolute

#### Maximum Ratings limits for SD_MODE. Ensuring that

#### VDD is always greater than VDDIO is one way to prevent

#### SD_MODE from violating the Absolute Maximum Ratings

#### limits. If this is not possible in the application (e.g., if VDD

#### < 3.0V and VDDIO = 3.3V), then it is necessary to add a

#### small resistance (~2kΩ) in series with SD_MODE to limit

#### the current into the SD_MODE pin. This is not a concern

#### when using the right channel or (left/2 + right/2) modes.

#### Figure 4 and Figure 5 show how to connect an external

#### resistor to SD_MODE when using an open-drain driver or

#### a push-pull driver.

## Table 4. Digital Filter Settings

## Table 5. SD_MODE Control

## Table 6. Examples of SD_MODE Pullup Resistor Values...............................................

```
LRCLK FREQUENCY -3dB CUTOFF
FREQUENCY
```
##### RIPPLE LIMIT CUTOFF

##### FREQUENCY

##### STOPBAND CUTOFF

##### FREQUENCY

##### STOPBAND

```
ATTENUATION (dB)
fLRCLK < 30kHz 0.446 x fLRCLK 0.443 x fLRCLK 0.464 x fLRCLK 75
30kHz < fLRCLK < 50kHz 0.47 x fLRCLK 0.43 x fLRCLK 0.58 x fLRCLK 60
fLRCLK > 50kHz 0.31 x fLRCLK 0.24 x fLRCLK 0.477 x fLRCLK 60
```
##### SD_MODE STATUS SELECTED CHANNEL

```
High VSD_MODE > B2 trip point Left
Pullup through RSMALL B2 trip point > VSD_MODE > B1 trip point Right
Pullup through RLARGE B1 trip point > VSD_MODE > B0 trip point (Left/2 + right/2)
Low B0 trip point > VSD_MODE Shutdown
```
```
LOGIC VOLTAGE LEVEL (VDDIO) (V) RSMALL (kΩ) RLARGE (kΩ)
1.8 69.8 300
3.3 210.2 634
```
MAX98357B
Class AB Performance


#### Startup

#### With the exception of BCLK = 256KHz, the only required

#### sequence for startup is that LRCLK must start within 1/

#### LRCLK period of BCLK starting.

#### When using a mode with BCLK = 256kHz, there are addi-

#### tional requirements for the part to power-up properly:

#### 1) BCLK and LRCLK cannot be applied before the part

#### is enabled.

#### 2) BCLK and LRCLK must start from logic low and tran-

#### sition to logic high.

#### 3) After VDD is > 2.3V AND SD_MODE is high, there

#### must be a 10μs wait time before starting BCLK and

#### LRCLK.

#### 4) LRCLK must start at least 1/2 BCLK after BCLK starts.

#### 5) LRCLK must start no more than 1/2 LRCLK after

#### BCLK starts.

#### 6) LRCLK must complete a full cycle; no partial LRCLK

#### cycles.

#### 7) Once started, BCLK and LRCLK must remain switch-

#### ing at 256kHz and 8kHz, respectively, and cannot

#### be interrupted during device operation. If BCLK and

#### LRCLK need to be stopped, SD_MODE must first be

#### set to 0V. Subsequent startups with BCLK = 256kHz

#### and LRCLK = 8kHz need to follow the sequence

#### described in steps 1-6.

#### Figure 6 shows an example where VDD reaches UVLO

#### maximum before SD_MODE is applied. In this example,

#### the 10μs wait time starts after SD_MODE is applied.

## Figure 4. SD_MODE Resistor Connected Using Open-Drain Driver

## Figure 5. SD_MODE Resistor Connected Using Push-Pull Driver........................................

```
GPIO
```
```
PROCESSOR VDDIO
```
```
R
```
```
100kΩ
±8%
```
```
LEFT MODE
```
```
RIGHT MODE
```
```
LEFT/2 + RIGHT/
MODE
```
```
B2 (1.4V typ)
```
```
B1 (0.77V typ)
```
```
B0 (0.16V typ)
```
```
VSD_MODE
```
```
MAX98357A
MAX98357B
```
```
GPIO
```
```
PROCESSOR
VDDIO
```
```
R
```
```
100kΩ
±8%
```
```
LEFT MODE
```
```
RIGHT MODE
```
```
LEFT/2 + RIGHT/
MODE
```
```
B2 (1.4V typ)
```
```
B1 (0.77V typ)
```
```
B0 (0.16V typ)
```
```
VSD_MODE
```
```
MAX98357A
MAX98357B
```
**MAX98357B**
Class AB Performance


#### I^2 S and Left Justified Mode.................................................................

#### The MAX98357A follows standard I^2 S timing by allowing

#### a delay of one BCLK cycle after the LRCLK transition

#### before the beginning of a new data word (Figure 7 and

#### Figure 8). The MAX98357B follows the left justified timing

#### specification by aligning the LRCLK transitions with the

#### beginning of a new data word (Figure 9 and Figure 10).

#### LRCLK ONLY supports 8kHz, 16kHz, 32kHz, 44.1kHz,

#### 48kHz, 88.2kHz, and 96kHz frequencies. LRCLK clocks

#### at 11.025kHz, 12kHz, 22.05kHz and 24kHz are NOT

#### supported. Do not remove LRCLK while BLCK is pres-

#### ent. Removing LRCLK while BCLK is present can cause

#### unexpected output behavior, including a large DC output

#### voltage.

#### The digital audio interface output mode is chosen by the

#### voltage at SD_MODE. Table 5 shows how the available

#### modes are selected. Trip point B0–B2 are shown the

#### Electrical Characteristics in the SD_MODE Comparator

#### Trip Points section. Values for SD_MODE pullup resistors

#### RSMALL and RLARGE are dependent on the voltage level

#### of VDDIO. See Table 6 for pullup resistor values.

#### TDM Mode..............................................................................

#### TDM mode is automatically detected by monitoring the

#### short channel sync pulse on LRCLK. The frequency

#### detector circuit detects the bit depth. In TDM mode,

#### the MAX98357A/MAX98357B has a fixed gain of 12dB.

#### GAIN_SLOT and SD_MODE are used to select to which

#### of 8 channels of TDM data the parts respond. Table 7

#### shows the connections for GAIN_SLOT and SD_MODE

#### for channel selection. The MAX98357A data is valid on

#### the BCLK rising edge. The MAX98357B data is valid on

#### the BCLK falling edge.

#### Figure 11, Figure 12, Figure 13, and Figure 14 show TDM

#### operation, in which a frame-sync pulse is used for LRCLK.

#### In TDM mode, there must be 128 (16-bit mode) or 256

#### (32-bit mode) BCLK cycles per frame. In TDM mode, the

#### ICs only accept 16-bit or 32-bit formatted data and any of

#### the 8 TDM slots can be selected.

## Figure 6. Required startup sequence when using BCLK = 256kHz

```
VDD = UVLO
MAXIMUM (2.3V)
```
```
BCLK
```
```
LRCLK
```
```
SD_MODE
```
```
½ BCLK < t < ½ LRCLK
```
```
½ LRCLK
```
```
> 10μs
```
```
t
```
```
> 10 μs
```
**MAX98357B**
Class AB Performance


## Figure 7. MAX98357A I^2 S Digital Audio Interface Timing, 16-Bit Resolution...............................

## Table 7. TDM Mode Channel Selection............................................................

```
SD_MODE GAIN_SLOT CHANNEL BITS
Low X Off N/A
VDD GND 0 16/
VDD VDD with 0Ω 1 16/
VDD Float 2 16/
VDD VDD with 100kΩ 3 16/
VDD GND with 100kΩ 4 16/
VDD through RLARGE GND 5 16/
VDD through RLARGE Float 6 16/
VDD through RLARGE VDD 7 16/
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
DIN D15D14D13D12D11D10D9D8 D7 D6 D5 D4 D3 D2 D1 D0D15D14D13D12D11D10D9 D8 D7D6 D5 D4 D3 D2 D1 D
```
```
16 BITS/CHANNEL SD_MODE = VDD
```
```
D15D
```
```
LEFT
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
IGNORED
```
```
DIN D15D14D13D12D11D10D9 D8D7 D6 D5 D4 D3 D2 D1 D0D15D14D13D12D11D10D9 D8 D7 D6 D5D4 D3 D2 D1 D
```
```
16 BITS/CHANNEL SD_MODE PULL UP THROUGH RSMALL (70K)
```
```
D15D
```
```
LEFT
```
```
IGNORED
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
DIN D15D14D13D12D11D10D9 D8D7 D6 D5 D4 D3 D2 D1 D0D15D14D13D12D11D10D9 D8 D7 D6 D5D4 D3 D2 D1 D
```
```
16 BITS/CHANNEL SD_MODE PULL UP THROUGH RLARGE (300k)
```
```
D15D
```
```
LEFT
```
**MAX98357B**
Class AB Performance


## Figure 8. MAX98357A I^2 S Digital Audio Interface Timing, 32-Bit Resolution...............................

```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
LRCLKBCLK
```
```
RIGHT
```
```
LEFT
```
```
DIN
```
```
IGNORED
```
```
LRCLKBCLK
```
```
RIGHT
```
```
LEFT
```
```
DIN
```
```
IGNORED
```
```
LRCLKBCLK
```
```
RIGHT
```
```
LEFT
```
```
DIN
```
```
LEFT AND RIGHT SUMMED
```
```
32 BITS/CHANNEL,
```
```
SD_MODE
```
```
= V
```
(^) DD
32 BITS/CHANNEL,
SD_MODE
PULLUP THROUGH R
SMALL
(70k)
32 BITS/CHANNEL,
SD_MODE
PULLUP THROUGH R
LARGE
(300k)
**MAX98357B**
Class AB Performance


## Figure 9. MAX98357B Left-Justified Digital Audio Interface Timing, 16-Bit Resolution.......................

```
D15D14D13D12D11D10D9 D8D7 D6D5 D4D3 D2D1D0D15D14D13D12D11D10D9D8 D7D6 D5D4 D3D2 D1D0D15D14
```
```
D15D14D13D12D11D10D9 D8D7 D6D5 D4D3 D2D1D0D15D14D13D12D11D10D9D8 D7D6 D5D4 D3D2 D1D0D15D14
```
```
D15D14D13D12D11D10D9 D8D7 D6D5 D4D3 D2D1D0D15D14D13D12D11D10D9D8 D7D6 D5D4 D3D2 D1D0D15D14
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
DIN
```
```
16 BITS/CHANNEL, SD_MODE = VDD
```
```
IGNORED
```
```
IGNORED
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
DIN
```
```
16 BITS/CHANNEL, SD_MODE PULLUP THROUGH RSMALL (70k)
```
```
LRCLK
```
```
BCLK
```
```
LEFT RIGHT
```
```
DIN
```
```
16 BITS/CHANNEL, SD_MODE PULLUP THROUGH RLARGE (300k)
```
```
LEFT AND RIGHT SUMMED
```
**MAX98357B**
Class AB Performance


## Figure 10. MAX98357B Left-Justified Digital Audio Interface Timing, 32-Bit Resolution......................

```
LRCLKBCLK
```
```
RIGHT
```
```
LEFT LEFT LEFT
```
```
DIN
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
32 BITS
```
```
/CHANNEL,
```
```
SD_MODE
```
```
= VDD
```
```
D31
```
```
D30
```
```
32 BITS
```
```
/CHANNEL,
```
```
SD_MODE
```
```
PULLUP THROUGH R
```
```
SMALL
```
```
(70k)
```
```
LEFT AND RIGHT SUMMED
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
IGNORED
```
```
D0
```
```
D31
```
```
LEFT D30
```
```
LRCLKBCLK
```
```
RIGHT
```
```
DIN
```
```
32 BITS
```
```
/CHANNEL,
```
```
SD_MODE
```
```
PULLUP THROUGH R
```
```
LARGE
```
```
(300k)
```
```
IGNORED
```
```
LRCLKBCLK
```
```
RIGHT
```
```
SDIN
```
```
D29
```
```
IGNORED
```
```
LEFT LEFT
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
BCLK
```
**MAX98357B**
Class AB Performance


## Figure 11. MAX98357A TDM 16-Bit DAI Timing

```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
IGNORED
```
```
IGNORED
```
```
D15
```
```
D14
```
```
LD3
```
```
D0
D1
TDM16-BIT DATA, 128-BIT FRAME, DATA IN CHANNELS 1-6
```
```
IGNORED
```
```
IGNORED
```
```
TDM16-BIT DATA, 128-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD
```
```
, GAIN_SLOT TIED TO GND, DATA IN CHANNEL 0
```
```
TDM16-BIT DATA, 128-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD
```
```
THROUGH RLARGE, GAIN_SLOT TIED TO V
```
```
DD
```
```
, DATA IN CHANNEL 7
```
**MAX98357B**
Class AB Performance


## Figure 12. MAX98357A TDM 32-Bit DAI Timing.....................................................

```
IGNORED
```
```
IGNORED
```
```
TDM32-BIT DATA, 256-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
THROUGH RLARGE, GAIN_SLOT TIED TO VDD
```
```
, DATA IN CHANNEL 7DD
```
```
TDM32-BIT DATA, 256-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD, GAIN_SLOT TIED TO GND, DATA IN CHANNEL 0
```
```
D31
```
```
D31
```
```
D30
```
```
D29
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D19
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
**MAX98357B**
Class AB Performance


## Figure 13. MAX98357B TDM 16-Bit DAI Timing.....................................................

```
TDM16-BIT DATA, 128-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD
```
```
, GAIN_SLOT TIED TO GND, DATA IN CHANNEL 0
```
```
TDM16-BIT DATA, 128-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD
```
```
THROUGH R
```
```
LARGE
```
```
, GAIN_SLOT TIED TO V
```
```
DD
```
```
, DATA IN CHANNEL 7
```
```
TDM16-BIT DATA, 128-BIT FRAME DATA IN CHANNELS 1-6
```
```
IGNORED
```
```
D15
```
```
D14
```
```
D13
```
```
D15
```
```
D14
```
```
LD3
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
IGNORED
```
```
D15
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D9
```
```
D8
```
```
D7
```
```
D6
```
```
D5
```
```
D4
```
```
D3
```
```
D2
```
```
D1
```
```
D0
```
```
D1
```
```
D0
```
```
IGNORED
```
```
IGNORED
```
**MAX98357B**
Class AB Performance


## Figure 14. MAX98357B TDM 32-Bit DAI Timing.....................................................

```
TDM32-BIT DATA, 256-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
, GAIN_SLOT TIED TO GND, DATA IN CHANNEL 0DD
```
```
TDM32-BIT DATA, 256-BIT FRAME
```
```
SD_MODE
```
```
TIED TO V
```
```
DD THROUGH R
```
```
LARGE
```
```
, GAIN_SLOT TIED TO V
```
```
DD
```
```
, DATA IN CHANNEL 7
```
```
IGNORED
```
```
IGNORED
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D09
```
```
D08
```
```
D07
```
```
D06
```
```
D05
```
```
D04
```
```
D03
```
```
D02
```
```
D01
```
```
D0
```
```
D31
```
```
D30
```
```
D29
```
```
D19
```
```
D31
```
```
D30
```
```
D29
```
```
D28
```
```
D27
```
```
D26
```
```
D25
```
```
D24
```
```
D23
```
```
D22
```
```
D21
```
```
D20
```
```
D18
```
```
D17
```
```
D16
```
```
D15
```
```
D14
```
```
D13
```
```
D12
```
```
D11
```
```
D10
```
```
D09
```
```
D08
```
```
D07
```
```
D06
```
```
D05
```
```
D04
```
```
D03
```
```
D02
```
```
D01
```
```
D0
```
```
D19
```
**MAX98357B**
Class AB Performance


### Class D Speaker Amplifier

#### The filterless Class D amplifier offers much higher effi-

#### ciency than Class AB amplifiers. The high efficiency of a

#### Class D amplifier is due to the switching operation of the

#### output stage transistors. Any power loss associated with

#### the Class D output stage is mostly due to the I^2 R loss of the

#### MOSFET on-resistance and quiescent current overhead.

#### Ultra-Low EMI Filterless Output Stage

#### Traditional Class D amplifiers require the use of external

#### LC filters, or shielding, to meet EN55022B electromagnet-

#### ic-interference (EMI) regulation standards. Maxim’s active

#### emissions-limiting edge-rate control circuitry and spread-

#### spectrum modulation reduces EMI emissions while main-

#### taining up to 92% efficiency.

#### Maxim’s spread-spectrum modulation mode flattens wide-

#### band spectral components while proprietary techniques

#### ensure that the cycle-to-cycle variation of the switching

#### period does not degrade audio reproduction or efficiency.

#### The ICs’ spread-spectrum modulator randomly varies the

#### switching frequency by ±20kHz around the center fre-

#### quency (300kHz). Above 10MHz, the wideband spectrum

#### looks like noise for EMI purposes (Figure 15).

#### Speaker Current Limit

#### If the output current of the speaker amplifier exceeds the

#### current limit (2.8A typ), the IC disables the outputs for

#### approximately 100μs. At the end of the 100μs, the outputs

#### are re-enabled. If the fault condition still exists, the IC con-

#### tinues to disable and reenable the outputs until the fault

#### condition is removed.

#### Gain Selection

#### The ICs offer five programmable gain selections through a

#### single gain input (GAIN_SLOT) in I^2 S/left justified mode.

#### Gain is referenced to the full-scale output of the DAC,

#### which is 2.1dBV (Table 8). In TDM mode, the gain is auto-

#### matically set at a fixed 12dB. Assuming that the desired

#### output swing is not limited by the supply voltage rail, the

#### IC’s output level can be calculated based on the digital

#### input signal level and selected amplifier gain according to

#### the following equation:

#### Output signal level (dBV) = input signal level (dBFS) +

#### 2.1dB + selected amplifier gain (dB)

#### where 0dBFS is referenced to 0dBV.

#### Click-and-Pop Suppression.................................................................

#### The IC speaker amplifier features Maxim’s comprehen-

#### sive click-and-pop suppression. During startup, the click-

#### and-pop suppression circuitry reduces audible transient

#### sources internal to the device by ramping the input signal

#### from mute to 0dB. When entering shutdown, the differen-

#### tial speaker outputs simultaneously drop to GND.

#### The comprehensive click-and-pop suppression of the

#### MAX98357 is unaffected by power-up or power-down

#### sequencing. Applying the DAI clocks before or after the

#### transition of SD_MODE yields the same click-and-pop

#### performance. The MAX98357 does not have a volume

#### ramp-down response when entering shutdown. For opti-

#### mal click-and-pop performance, ramp down the digital

#### data on SDIN before powering down the MAX98357.

## Figure 15. EMI with 12in of Speaker Cable and No Output Filtering......................................

## Table 8. Gain Selection

```
GAIN_SLOT I^2 S/LJ GAIN (dB)
Connect to GND through 100kΩ
±5% resistor^15
Connect to GND 12
Unconnected 9
Connect to VDD 6
Connect to VDD through 100kΩ
±5% resistor
```
##### 3

```
FREQUENCY (MHz)
```
```
EMISSIONS LEVEL (dBμV/m)
```
```
100200300400500600700800900
```
```
10
```
```
30
```
```
50
```
```
70
```
```
90
```
```
-10
0 1000
```
MAX98357B
Class AB Performance


## Figure 16. Left-Channel PCM Operation with 6dB Gain...............................................

## Figure 17. Left-Channel PCM Operation with 12dB Gain

## Applications Information........................................................................

```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
*RESPONDS TO LEFT CHANNEL WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
A1B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
*RESPONDS TO LEFT CHANNEL WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
GND
```
```
A1B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
**MAX98357B**
Class AB Performance


## Figure 18. Right-Channel PCM Operation with 6dB Gain..............................................

## Figure 19. (Left/2 + Right/2) PCM Operation with 6dB Gain............................................

```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
RSMALL
(69.8kΩ)**
```
```
DIN
```
```
GND
```
```
*RESPONDS TO RIGHT CHANNEL WHEN GPIO IS HIGH.
**69.8kΩ ASSUMES VGPIO = 1.8V.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
A1B2 A2
```
```
C1
```
```
C3
```
```
B1
C2
```
```
B3
```
```
A3
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
RLARGE
(300kΩ)**
```
```
DIN
```
```
GND
```
```
*LEFT AND RIGHT CHANNELS SUMMED WHEN GPIO IS HIGH.
**300kΩ ASSUMES VGPIO = 1.8V.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
B2 A2
A1
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
**MAX98357B**
Class AB Performance


## Figure 20. Stereo PCM Operation Using Two ICs....................................................

```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
RSMALL
(69.8kΩ)**
```
```
DIN
```
```
GND
```
```
*RESPONDS TO RIGHT CHANNEL WHEN GPIO IS HIGH.
**69.8kΩ ASSUMES VGPIO = 1.8V.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
*RESPONDS TO CHANNEL 0 WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
A1B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
A1B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
**MAX98357B**
Class AB Performance


## Figure 21. Channel TDM Operation (Gain Fixed at 12dB)..............................................

```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
0.1μF 10μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
*RESPONDS TO CHANNEL 1 WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
*RESPONDS TO CHANNEL 2 WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
MAX98357A
MAX98357B
```
```
A1 B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
0.1μF 10μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
MAX98357A
MAX98357B
```
```
A1 B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
```
```
0.1μF 10μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
MAX98357A
MAX98357B
```
```
A1 B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
*RESPONDS TO CHANNEL 3 WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
```
OUTP
```
```
OUTN
```
```
GAIN_SLOT VDD
```
```
2.5V TO 5.5V
100kΩ
0.1μF 10μF
```
```
SD_MODE
```
```
BCLK
```
```
LRCLK
```
```
DIN
```
```
GND
```
```
MAX98357A
MAX98357B
```
```
A1 B2 A2
```
```
C1
```
```
C3
```
```
B1 C2
```
```
B3
```
```
A3
```
```
GPIO*
```
```
CODEC
```
```
BIT CLOCK
```
```
FRAME CLOCK
```
```
DATA OUT
```
```
*RESPONDS TO CHANNEL 0 WHEN GPIO IS HIGH.
THE MAX98357A/B ARE SHUT DOWN WHEN GPIO IS LOW.
```
**MAX98357B**
Class AB Performance


### Filterless Class D Operation...................................................................

#### Traditional Class D amplifiers require an output filter

#### to recover the audio signal from the amplifier’s output.

#### The filter adds cost, size, and decreases efficiency

#### and THD+N performance. The ICs’ filterless modulation

#### scheme does not require an output filter. The device relies

#### on the inherent inductance of the speaker coil and the

#### natural filtering of both the speaker and the human ear to

#### recover the audio component of the square-wave output.

#### Because the switching frequency of the ICs is well beyond

#### the bandwidth of most speakers, voice coil movement due

#### to the switching frequency is very small. Use a speaker

#### with a series inductance > 10μH. Typical 8Ω speakers

#### exhibit series inductances in the 20μH to 100μH range.

### Power-Supply Input..........................................................................

#### VDD, which ranges from 2.5V to 5.5V, powers the IC,

#### including the speaker amplifier. Bypass VDD with a 0.1μF

#### and 10μF capacitor to GND. Some applications might

#### require only the 10μF bypass capacitor, making it pos-

#### sible to operate with a single external component. Apply

#### additional bulk capacitance at the ICs if long input traces

#### between VDD and the power source are used.

### Layout and Grounding........................................................................

#### Proper layout and grounding are essential for optimum

#### performance. Good grounding improves audio perfor-

#### mance and prevents switching noise from coupling into

#### the audio signal.

#### Use wide, low-resistance output traces. As load imped-

#### ance decreases, the current drawn from the device out-

#### puts increases. At higher current, the resistance of the

#### output traces decreases the power delivered to the load.

#### For example, if 2W is delivered from the speaker output to

#### a 4Ω load through 100mΩ of total speaker trace, 1.904W

#### is being delivered to the speaker. If power is delivered

#### through 10mΩ of total speaker trace, 1.951W is being

#### delivered to the speaker. Wide output, supply, and ground

#### traces also improve the power dissipation of the ICs.

#### Parasitic capacitance on the output traces cause higher

#### quiescent current by VDD x 300kHz x CPARASITIC.

#### For example, at VDD = 5V and a total parasitic capaci-

#### tance of 100pF (50pF on each output trace), the increase

#### in quiescent current is 5 x 300kHz x 100pF = 150μA.

#### The ICs are inherently designed for excellent RF immu-

#### nity. For best performance, add ground fills around all

#### signal traces on top or bottom PCB planes.

## Functional Diagram

```
2.5V TO 5.5V
```
```
10μF 0.1μF
```
```
LRCLK
BCLK
DIN
SD_MODE INTERPOLATOR DAC
```
```
CLASS D
OUTPUT
STAGE
```
```
OUTP
```
```
VDD GAIN_SLOT
```
```
OUTN
```
```
DIGITAL
AUDIO
INTERFACE
```
```
C3
```
```
B1
```
```
C1
```
```
A1
```
```
C2
```
```
A2 B2
```
```
A3
```
```
B3
```
```
MAX98357A
MAX98357B
```
```
GND
```
**MAX98357B**
Class AB Performance


#### Gains of 6dB, 9dB, and 12dB are selectable without

#### using a via or routing out the center bump of the WLP.

#### This simplifies the layout and allows for inexpensive PCB

#### fabrication. Here is a layout example with the gain set to

#### 12dB. The center bump is tied to the adjacent GND pin.

#### Refer to Application Note 6643: Optimize Cost, Size, and

#### Performance with MAX98357 WLP for more information.

#### In many applications, the only passive component required

#### would be a single capacitor which results in a tiny solution

#### size of 4.32mm^2.

### WLP Applications Information..................................................................

#### For the latest application details on WLP construction,

#### dimensions, tape carrier information, PCB techniques,

#### bump-pad layout, and recommended reflow temperature

#### profile, as well as the latest information on reliability test-

#### ing results, refer to the Application Note 1891: Wafer-

#### Level Packaging (WLP) and Its Applications. Figure 24

#### shows the dimensions of the WLP balls used on the ICs.

## Figure 24. MAX98357A/MAX98357B WLP Ball Dimensions

## Figure 22. WLP Pin Connect for set 12dB Gain Without Via............................................

## Figure 23. Example Layout Configured for Left-Channel Audio and Gain of 12dB...........................

+ _Denotes a lead(Pb)-free/RoHS-compliant package._
T _= Tape and reel._
/V _denotes an automotive-qualified part._

##### PART TEMP RANGE PIN-PACKAGE TOP MARK

```
MAX98357A ETE+ -40°C to +85°C 16 TQFN +AKK
MAX98357AETE+T -40°C to +85°C 16 TQFN +AKK
MAX98357AEWL+T -40°C to +85°C 9 WLP +AKM
MAX98357AGTE/V+ -40°C to +105°C 16 TQFN +AKV
MAX98357B ETE+ -40°C to +85°C 16 TQFN +AKL
MAX98357BETE+T -40°C to +85°C 16 TQFN —
MAX98357BEWL+T -40°C to +85°C 9 WLP +AKN
```
## Ordering Information

```
1.78mm
```
```
2.49mm
```
```
0.21mm
```
```
0.24mm
```
MAX98357B
Class AB Performance


##### PACKAGE TYPE PACKAGE CODE OUTLINE NO. LAND PATTERN NO.

```
9 WLP W91F1+1 21-0896 Refer to Application Note 1891
16 TQFN T1633+4 21-0136 90-0031
```
## Package Information

For the latest package outline information and land patterns (footprints), go to **[http://www.analog.com/packages](http://www.analog.com/packages)**. Note that a “+”, “#”, or “-”
in the package code indicates RoHS status only. Package drawings may show a different suffix character, but the drawing pertains to
the package regardless of RoHS status.

# integratedTM

# maxim

```
0.05 AB
```
```
0.05 S
```
```
TIE WANG 06/27/14
```
## MAX98357B

## Class AB Performance


## Package Information (continued)

For the latest package outline information and land patterns (footprints), go to **[http://www.analog.com/packages](http://www.analog.com/packages)**. Note that a “+”, “#”, or “-”
in the package code indicates RoHS status only. Package drawings may show a different suffix character, but the drawing pertains to
the package regardless of RoHS status.

MAX98357B
Class AB Performance


## Package Information (continued)

For the latest package outline information and land patterns (footprints), go to **[http://www.analog.com/packages](http://www.analog.com/packages)**. Note that a “+”, “#”, or “-”
in the package code indicates RoHS status only. Package drawings may show a different suffix character, but the drawing pertains to
the package regardless of RoHS status.

MAX98357B
Class AB Performance


##### REVISION

##### NUMBER

##### REVISION

##### DATE

##### DESCRIPTION PAGES

##### CHANGED

```
0 9/13 Initial release —
```
```
1 11/13 Added two new TOCs, replaced TOC 29, updated Figures 1–3, and made various
corrections
```
##### 1, 4–20,

##### 29–32, 34

```
2 8/14 Added THD+N for TQFN package with typical spec 5
3 1/15 Updated spread-spectrum bandwidth spec 5, 28
4 2/15 Added automotive-qualified part 34
5 6/15 Updated TOCs 30a and 30b 12
6 8/15 Corrected package outline for WLP package 36
7 2/16 Removed future product designations 34
8 6/16 Removed future product designation on MAX98357AGTE/V+ 34
```
```
9 7/17
```
```
Updated dynamic range and output noise specifications in Electrical Characteristics
table^5
10 8/17 Updated soldering temperature in the Absolute Maximum Ratings section 4
```
##### 11 5/18

```
Updated General Description , Features , and Applications sections, changed Class D
Switching Frequency in Electrical Characteristics table and other sections, replaced
TOC 20, added DC blocker information to Detailed Description section, updated and
added figures to Layout and Grounding section
```
##### 1, 5, 11, 16,

##### 28, 33, 34

##### 12 4/19

```
Updated Features section to match Electrical Characteristics table typical values.
Added Startup section and new Figure 6 for startup requirements when using
BCLK = 256kHz
```
##### 1, 17–34

```
13 7/19 Updated TOCs 05 and 12 11, 12
14 10/19 Updated Ordering Information table 34
15 1/26 Updated Ordering Information table 34
16 2/26 Updated Ordering Information table 34
```
## Revision History..............................................................................

**MAX98357B**
Class AB Performance

```
Information furnished by Analog Devices is believed to be accurate and reliable. However, no responsibility is assumed
by Analog Devices for its use, nor for any infringements of patents or other rights of third parties that may result from its
use. Specifications subject to change without notice. No license is granted by implication or otherwise under any patent
or patent rights of Analog Devices. Trademarks and registered trademarks are the property of their respective owners.
All Analog Devices products contained herein are subject to release and availability.
```

