## 1.5MHz, 1A Synchronous Step-Down Regulator

```
This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: http://www.feeling-tech.com.tw Rev. 0.
```
```
FP
```
```
GND
```
```
SW
```
```
FB / VOUT
```
```
VIN
```
```
RUN
```
```
VIN VOUT
```
### General Description

```
The FP6161 is a high efficiency current mode synchronous buck PWM DC-DC regulator. The
internal generated 0.6V precision feedback reference voltage is designed for low output voltage. Low
RDS (ON) synchronous switch dramatically reduces conduction loss. To extend battery life for portable
application, 100% duty cycle is supported for low-dropout operation. Shutdown mode also helps saving
the current consumption. The FP6161 is packaged in DFN-6L, SOT23-5L, and TSOT23-5L to reduce
PCB space.
```
### Features

```
 Input Voltage Range: 2.5 to 5.5V
 Precision Feedback Reference Voltage: 0.6V (±2%)
 Output Current: 1A (Max.)
 Duty Cycle: 0~100%
 Internal Fixed PWM Frequency: 1.5MHz
 Low Quiescent Current: 100μA
 No Schottky Diode Required
 Built-in Soft Start
 Current Mode Operation
 Over Temperature Protection
 Package: DFN-6L (2x2mm), SOT23-5L, TSOT23-5L
```
### Applications

```
 Cellular Telephone
 Wireless and DSL Modems
 Digital Still Cameras
 Portable Products
 MP3 Players
```
### Typical Application Circuit


```
This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: http://www.feeling-tech.com.tw Rev. 0.
```
## Function Block Diagram

```
Switching
Control Logic
```
```
CurrentSense
Current
Slope Limit
Compensation OSC
```
```
UVLO
```
```
ShutdownControl
```
```
OTP
```
```
Reference
GeneratorVoltage
```
```
FBUVCom-
```
-

```
+
parator
```
```
ErrorAmp.
```
-

```
+
```
```
RUN
```
```
Pre-Driver and
SR Latch Anti Shoot-through
```
```
S Q
R Q
```
-

```
+PWM
paratorCom-
```
```
Reverse
```
```
GND
```
```
VVFB/FB/OUTOUT
```
```
0.6V
```
```
0.3V
```
```
SW
```
```
VCC
```
```
Reverse Current
Detector
```
## Pin Descriptions

##### DFN-6L

## 986 ZYa

## SOT23-5L / TSOT23-5L

# ZY

```
Name No. I / O Description
NC 1 No Connect
RUN 2 I Enable Pin
VIN 3 P Power Supply
SW 4 O Switch
GND 5 P Ground
FB / VOUT 6 I Feedback
EP 7 P Exposed PAD – Must Connect to Ground
```
```
Name No. I / O Description
RUN 1 I Enable
GND 2 P Ground
SW 3 O Switch
VIN 4 P Power Supply
FB / VOUT 5 I Feedback
```

```
This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: http://www.feeling-tech.com.tw Rev. 0.
```
### Marking Information

##### DFN-6L SOT23-5L / TSOT23-5L

```
Halogen Free: Halogen free product indicator
Lot Number: Wafer lot number’s last two digits
For Example: 132386TB  86
Part Number Code: Part number identification code for this product. It should be always “ZY”.
Year: Production year’s last digit
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Ordering Information

```
Part Number Code Operating Temperature Package MOQ Description
```
FP6161dR-LF-ADJ ZY -40°C ~ +85°C (^) (2x2mm) DFN-6L 2500EA Tape & Reel
FP6161KR-LF-ADJ ZY -40°C ~ +85°C SOT23-5L 3000EA Tape & Reel
FP6161iR-LF-ADJ ZY -40°C ~ +85°C TSOT23-5L 3000EA Tape & Reel

### Absolute Maximum Ratings

```
Parameter Symbol Conditions Min. Typ. Max. Unit
Input Supply Voltage VIN -0.3 6 V
RUN, VFB, SW Pin Voltage -0.3 VIN V
P-Channel Switch Source Current (DC) 1.5 A
N-Channel Switch Source Current (DC) 1.5 A
Peak SW Switch Sink and Source Current (AC) 2 A
DFN-6L +165 °C / W
Thermal Resistance (Junction to Ambient) θJA SOT23-5L +250 °C / W
TSOT23-5L +250 °C / W
DFN-6L +20 °C / W
```
Thermal Resistance (Junction to Case) θJC (^) SOT23-5L +90 °C / W
TSOT23-5L +90 °C / W
Junction Temperature +150 °C
Storage Temperature -65 +150 °C
DFN-6L 750 mW
Allowable Power Dissipation PD SOT23-5L 500 mW
TSOT23-5L 500 mW
Lead Temperature (soldering, 10 sec) +260 °C

### Suggested IR Re-flow Soldering Curve


```
This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: http://www.feeling-tech.com.tw Rev. 0.
```
### Recommended Operating Conditions

```
Parameter Symbol Conditions Min. Typ. Max. Unit
Supply Voltage VIN 2.5 5.5 V
Operating Temperature -40 +85 °C
```
### DC Electrical Characteristics (TA= 25°C, VIN=3.6V, unless otherwise noted)

```
Parameter Symbol Conditions Min. Typ. Max. Unit
Regulated Feedback Voltage VFB TA=25°C^ 0.588 0.6 0.612 V
-40°C ~+85°C 0.582 0.6 0.618 V
Line Regulation with VREF VFB VIN=2.5V to 5.5V 0.04 0.4  / V
Output Voltage Line Regulation VOUT VIN=2.5 to 5.5V 0.04 0.4 % / V
RDS (ON) of P-Channel FET RDS(ON) PISW=100mA 0.28 0.35 Ω
RDS (ON) of N-Channel FET RDS (ON) NISW =-100mA 0.25 0.32 Ω
SW Leakage ILSW VRUN=0V, VIN=5V ±0.01 ±1 μA
Peak Inductor Current IPK VFB=0.5V 1.125 1.5 1.875 A
Quiescent Current ICC Shutdown, VRUN=0V 0.1 1 μA
Active, VFB=0.5V, VRUN=VIN 100 μA
RUN Threshold VRUN 0.3 1 1.5 V
RUN Leakage Current IRUN ±0.01 ±1 μA
Oscillator Frequency FOSC VFB=0.6V 1.2 1.5 1.8 MHz
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Typical Operating Characteristics

(TA= 25°C, VIN=3.6V, unless otherwise noted)
Supply Current vs. VIN

```
65
```
```
70
```
```
75
```
```
80
```
```
85
```
```
90
```
```
95
```
```
100
```
```
23456
VIN (V)
```
```
Supply Current (μA
```
```
)
85°C
```
```
25°C
```
```
-45°C
```
```
VFB=0.5V
```
```
Supply Current vs. VIN
```
```
0
```
```
2
```
```
4
```
```
6
```
```
8
```
```
10
12
```
```
14
16
```
```
18
```
```
23456
VIN (V)
```
```
Supply Current (nA)
```
```
85 °C
```
```
25 °C -45°C
```
```
Shutdown
```
```
Reference Voltage vs. Temperature
```
```
0.
0.
```
```
0.
0.
0.
```
```
0.
0.
```
```
0.
0.
0.
```
```
0.
```
```
-60-50-40-30-20-10 0 102030405060708090
Temperature (°C)
```
```
Reference Voltage (V)
```
```
VIN=3.6V
```
```
Frequency vs. Temerature
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
-50 -40 -30 -20 -10 0 10 20 30 40 50 60 7080 90
Temperature (°C)
```
```
Frequency (MHz
```
```
)
```
```
TA=25°C
```
```
Supply Current vs. VIN
```
```
18
```
```
19
```
```
20
```
```
21
```
```
22
```
```
23
```
```
24
```
```
23456
VIN (V)
```
```
Supply Current (μA
```
```
)
85°C
25°C -45°C
```
```
VFB=0.7V
```
```
Line Regulation
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
23456
VIN (V)
```
```
Reference Voltage (V)
```
```
TA=25°C
```
```
Frequency vs. VIN
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
1.
```
```
23456
VIN (V)
```
```
Frequency (MHz
```
```
)
```
```
VI N=3.6V
```
```
Switch Leakage vs. Input Volatge
```
```
0
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
1
```
```
1.
```
```
1234567
VIN (V)
```
```
Switch Leakage (nA)
```
```
Synchronous Switch
Main Switch
```
```
TA=25°C
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Function Description

##### Control Loop

```
The FP6161 is a high efficiency current mode synchronous buck regulator. Both the main
(P-channel MOSFET) and synchronous (N-channel MOSFET) switches are built internally. With
current mode operation, the PWM duty is controlled both by the error amplifier output and the peak
inductor current. At the beginning of each cycle, the oscillator turn on the P-MOSFET switch to
source current from VIN to SW output. Then, the chip starts to compare the inductor current with the
error amplifier output. Once the inductor current is larger than the error amplifier output, the
P-MOSFET switch is turned off. When the load current increases, the feedback voltage FB will
slightly drop. This causes the error amplifier to output a higher current level until the prior mentioned
peak inductor current reach the same level. The output voltage then can be sustained at the same.
When the top P-MOSFET switch is off, the bottom synchronous N-MOSFET switch is turned on.
Once the inductor current reverses, both top and bottom MOSFET will be turn off to leave the SW pin
into high impedance state.
The FP6161’s current mode control loop also includes slope compensation to suppress
sub-harmonic oscillations at high duty cycles. This slope compensation is achieved by adding a
compensation ramp to the inductor current signal.
```
##### LDO Mode

```
The FP6161’s maximum duty cycle can reach 100%. That means the driver’s main switch is
turn on through out whole clock cycle. Once the duty reaches 100%, the feedback path no longer
controls the output voltage. The output voltage will be the input voltage minus the main switch
voltage drop.
```
##### Over Current Protection

FP6161 limits the peak main switch current cycle by cycle. When over current occurs, chip will
turn off the main switch and turn the synchronous switch on until next cycle.

##### Short Circuit Protection

When the FB pin drops below 300mV, the chip will tri-state the output pin SW automatically. After
300us rest to avoid over heating, chip will re-initiate PWM operation with soft start.

##### Thermal Protection

```
FP6161 will shutdown automatically when the internal junction temperature reaches 150°C to
```
#### protect both the part and the system.


This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Application Information

##### Input Capacitor Selection

The input capacitor must be connected to the VIN pin and GND pin of FP6161 to maintain steady
input voltage and filter out the pulsing input current. The voltage rating of input capacitor must be
greater than maximum input voltage plus ripple voltage.
In switch mode, the input current is discontinuous in a buck converter. The source current
waveform of the high-side MOSFET is a square wave. To prevent large voltage transients, a low ESR
input capacitor sized for the maximum RMS current must be used. The RMS value of input capacitor
current can be calculated by:

```

```
###### 

###### 

######   

```
 IN
O
IN
RMS O O
V
```
###### 1 V

###### V

###### I I V

###### MAX^

It can be seen that when VO is half of VIN, CIN is under the worst current stress. The worst current
stress on CIN is IO_MAX / 2.

##### Inductor Selection

The value of the inductor is selected based on the desired ripple current. Large inductance gives
low inductor ripple current and small inductance result in high ripple current. However, the larger value
inductor has a larger physical size, higher series resistance, and / or lower saturation current. In
experience, the value is to allow the peak-to-peak ripple current in the inductor to be 10%~20%
maximum load current. The inductance value can be calculated by:

### IN

```
O
O
```
```
IN O
IN
```
```
O
L
```
```
IN O
V
```
###### V

```
f 2 ( 10 %~ 20 %)I
```
###### (V V )

###### V

###### V

```
f I
```
###### L (V V )

######  

######  

###### 

######  

```
The inductor ripple current can be calculated by:
```
```

```
###### 

###### 

######  

######    IN

```
L O O
V
```
###### 1 V

```
f L
```
### I V

Choose an inductor that does not saturate under the worst-case load conditions, which is the
load current plus half the peak-to-peak inductor ripple current, even at the highest operating
temperature. The peak inductor current is:

```
2
```
###### I I IL

```
L_PEAK O
```
######  


This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

The inductors in different shape and style are available from manufacturers. Shielded inductors
are small and radiate less EMI issue. But they cost more than unshielded inductors. The choice
depends on EMI requirement, price and size.

```
Inductor Value (μH) Dimensions Component Supplier Model
2.2 4.2×3.7×1.2 FENG-JUI TP4212-2R2M
2.2 4.4×5.8×1.2 Sumida CMD4D11 2R
3.3 4.2×3.7×1.2 FENG-JUI TP4212-3R3M
4.7 4.2×3.7×1.2 FENG-JUI TP4212-4R7M
4.7 4.4×5.8×1.2 Sumida CMD4D11 4R
4.7 4.9×4.9×1.0 Sumida CLSD09 4R
```
##### Output Capacitor Selection

The output capacitor is required to maintain the DC output voltage. Low ESR capacitors are
preferred to keep the output voltage ripple low. In a buck converter circuit, output ripple voltage is
determined by inductor value, switching frequency, output capacitor value and ESR. The output ripple
is determined by:

```

```
###### 

###### 

###### 

```
 OL COUT 8 fCOUT
V I ESR^1
```
Where f = operating frequency, COUT= output capacitance and ΔIL = ripple current in the inductor.
For a fixed output voltage, the output ripple is highest at maximum input voltage since ΔIL increases
with input voltage.

```
Capacitor Value (μF) Case Size Component Supplier Model
4.7 0603 TDK C1608JB0J475M
10 0805 Taiyo Yuden JMK212BJ106MG
10 0805 TDK C12012X5ROJ106K
22 0805 1206 TDK C2012JB0J226M
```
##### Using Ceramic Input and Output Capacitors

Care must be taken when ceramic capacitors are used at the input and the output. When a
ceramic capacitor is used at the input and the power is supplied by a wall adapter through long wires, a
load step at the output can induce ringing at the input, VIN. At best, this ringing can couple to the output
and be mistaken as loop instability. At worst, a sudden inrush current through the long wires can
potentially cause a voltage spike at VIN, which may large enough to damage the part. When choosing
the input and output ceramic capacitors, choose the X5R or X7R specifications. Their dielectrics have
the best temperature and voltage characteristics of all the ceramics for a given value and size.


This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

##### Output Voltage Programming

In the adjustable version, the output voltage is set using a resistive voltage divider from the output
voltage to FB. The output voltage is:

```

```
###### 

###### 

######   

```
2
O^1
R
```
###### V 0. 6 V 1 R

##### The recommended resistor value is summarized below:

###### VOUT (V) R 1 (Ω) R 2 (Ω) C 3 (F)

```
0.6 200k Not Used Not Used
1.2 200k 200k 10p
1.5 300k 200k 10p
1.8 200k 100k 10p
2.5 270k 85k 10p
3.3 306k 68k 10p
```
##### PC Board Layout Checklist

1. The power traces, consisting of the GND, SW and VIN trace should be kept short, direct and
    wide.
2. Place CIN near VIN pin as closely as possible to maintain input voltage steady and filter out the
    pulsing input current.
3. The resistive divider R 1 and R 2 must be connected to FB pin directly and as closely as possible.
4. FB is a sensitive node. Please keep it away from switching node, SW. A good approach is to
    route the feedback trace on another PCB layer and have a ground plane between the top and
    feedback trace routing layer. This reduces EMI radiation on to the DC-DC converter its own
    voltage feedback trace.
5. Keep the GND plates of CIN and COUT as close as possible. Then connect this to the ground
    plane (if one is used) with several vias. This reduces ground plane noise by preventing the
    switching currents from circulating through the ground plane. It also reduces ground bounce at
    FP6161 by giving it a low impedance ground connection.


This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

##### Suggested Layout for SOT23-5L

##### Suggested Layout for DFN-6L


This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Typical Application

```
FP
```
```
GND
```
```
SW
```
```
FB / VOUT
```
```
VIN
```
```
RUN
```
```
3
```
```
2
```
```
5
```
##### VIN^4

```
2.5 V~5.5 V
```
```
SOT23-5L / TSOT23-5L
```
```
270KR
```
```
85KR
```
```
10μFC
```
```
L1 3.3uH
```
```
10pFC
10μFC1^1
```
##### VOUT

```
2.5 V / 1A
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

##### ILOAD: 100mA~1A

#### Ch1:V^

```
OUT Ch2: ISW
```
##### EN On waveform (VOUT: 1.8V)

#### Ch1: EN Ch2: SW Ch3: V^

```
OUT Ch4: ISW
```
##### Efficiency (VIN: 5.3V)

```
Efficiency VS Output Current
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
0.1 1.0 10.0 100.0 1000.0Output Current (mA)
```
```
Efficiency (%)
```
```
Vout=3.3V
Vout=1.8V
Vout=1.2V
```
#### ILOAD: 200mA~1A

#### Ch1: V^

```
OUT Ch2: ISW
```
##### Efficiency (VOUT: 2.5V)

```
Efficiency VS. Output Current
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
0.1 1.0 Output Current (mA)10.0 100.0 1000.
```
```
Efficiency (%)
```
```
Vin=2.7V
Vin=3.6V
Vin=4.2V
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

### Package Outline

##### DFN-6L

```
Unit: MM
Symbols Min. (mm) Max. (mm)
A 0.700 0.
A1 0.000 0.
b 0.200 0.
c 0.190 0.
D 1.950 2.
D2 1.350 1.
E 1.950 2.
E2 0.750 0.
e 0.650 REF
L 0.300 0.
y 0.000 0.
```

This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: [http://www.feeling-tech.com.tw](http://www.feeling-tech.com.tw) Rev. 0.

##### SOT23-5L

```
Unit: MM
```
##### Note:

1. Package dimensions are in compliance with JEDEC outline: MO-178 AA.
2. Dimension “D” does not include molding flash, protrusions or gate burrs.
3. Dimension “E1” does not include inter-lead flash or protrusions.

```
Symbols Min. (mm) Max.(mm)
A 1.050 1.
A1 0.050 0.
A2 1.000 1.
b 0.250 0.
c 0.080 0.
D 2.700 3.
E 2.600 3.
E1 1.500 1.
e 0.950 BSC
e1 1.900 BSC
L 0.300 0.
L1 0.600 REF
L2 0.250 BSC
θ° 0 ° 10 °
θ1° 3 ° 7°
θ2° 6° 10 °
```

```
This datasheet contains new product information. Feeling Technology reserves the rights to modify the product specification without notice.
No liability is assumed as a result of the use of this product. No rights under any patent accompany the sales of the product.
Website: http://www.feeling-tech.com.tw Rev. 0.
```
##### TSOT23-5L

```
Unit: MM
```
##### Note:

1. Dimension “D” does not include molding flash, protrusions or gate burrs.
2. Dimension “E1” does not include inter-lead flash or protrusions.

```
Symbols Min.(mm) Max.(mm)
A 0.750 0.
A1 0.000 0.
A2 0.700 0.
b 0.350 0.
c 0.100 0.
D 2.800 3.
E 2.600 3.
E1 1.500 1.
e 0.950 BSC
e1 1.900 BSC
L 0.370 0.
L1 0.600 REF
L2 0.250 BSC
R 0.
R1 0.100 0.
θ° 0 ° 8 °
θ 1 4 ° 12 °
```

