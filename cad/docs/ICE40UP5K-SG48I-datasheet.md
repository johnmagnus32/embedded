# Data Sheet

#### FPGA-DS- 02008 - 1.

December 2020


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
**Disclaimers**

Lattice makes no warranty, representation, or guarantee regarding the accuracy of information contained in this document or the suitability of its
products for any particular purpose. All information herein is provided AS IS and with all faults, and all risk associated with such information is entirely
with Buyer. Buyer shall not rely on any data and performance specifications or parameters provided herein. Products sold by Lattice have been
subject to limited testing and it is the Buyer's responsibility to independently determine the suitability of any products and to test and verify the
same. No Lattice products should be used in conjunction with mission- or safety-critical or any other application in which the failure of Lattice’s
product could create a situation where personal injury, death, severe property or environmental damage may occur. The information provided in this
document is proprietary to Lattice Semiconductor, and Lattice reserves the right to make any changes to the information in this document or to any
products at any time without notice.


## Data Sheet

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
- Acronyms in This Document Contents
- 1. General Description
   - 1.1. Features
- 2. Product Family
   - 2.1. Overview
- 3. Architecture
   - 3.1. Architecture Overview
      - 3.1.1. PLB Blocks
      - 3.1.2. Routing
      - 3.1.3. Clock/Control Distribution Network
      - 3.1.4. sysCLOCK Phase Locked Loops (PLLs)
      - 3.1.5. sysMEM Embedded Block RAM Memory
      - 3.1.6. sysMEM Single Port RAM Memory (SPRAM)
      - 3.1.7. sysDSP
      - 3.1.8. sysI/O Buffer Banks
      - 3.1.9. sysI/O Buffer
      - 3.1.10. On-Chip Oscillator
      - 3.1.11. User I^2 C IP
      - 3.1.12. User SPI IP
      - 3.1.13. RGB High Current Drive I/O Pins
      - 3.1.14. RGB PWM IP
      - 3.1.15. Non-Volatile Configuration Memory
   - 3.2. iCE40 UltraPlus Programming and Configuration
      - 3.2.1. Device Programming
      - 3.2.2. Device Configuration
      - 3.2.3. Power Saving Options
- 4. DC and Switching Characteristics
   - 4.1. Absolute Maximum Ratings
   - 4.2. Recommended Operating Conditions
   - 4.3. Power Supply Ramp Rates
   - 4.4. Power-On Reset
   - 4.5. Power-up Supply Sequence...............................................................................................................................
   - 4.6. External Reset
   - 4.7. Power-On-Reset Voltage Levels
   - 4.8. ESD Performance
   - 4.9. DC Electrical Characteristics
   - 4.10. Supply Current
   - 4.11. User I^2 C Specifications
   - 4.12. I^2 C 50 ns Delay
   - 4.13. I^2 C 50 ns Filter
   - 4.14. User SPI Specifications
   - 4.15. Internal Oscillators (HFOSC, LFOSC)
   - 4.16. sysI/O Recommended Operating Conditions
   - 4.17. sysI/O Single-Ended DC Electrical Characteristics
   - 4.18. Differential Comparator Electrical Characteristics
   - 4.19. Typical Building Block Function Performance
      - 4.19.1. Pin-to-Pin Performance (LVCMOS25)
      - 4.19.2. Register-to-Register Performance
   - 4.20. sysDSP Timing
   - 4.21. SPRAM Timing
   - 4.22. Derating Logic Timing
   - 4.23. Maximum sysI/O Buffer Performance
   - 4.24. iCE40 UltraPlus Family Timing Adders All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
   - 4.25. iCE40 UltraPlus External Switching Characteristics
   - 4.26. sysCLOCK PLL Timing
   - 4.27. SPI Master or NVCM Configuration Time
   - 4.28. sysCONFIG Port Timing Specifications
   - 4.29. RGB LED Drive
   - 4.30. Switching Test Conditions
- 5. Pinout Information
   - 5.1. Signal Descriptions
      - 5.1.1. Power Supply Pins
      - 5.1.2. Configuration Pins
      - 5.1.3. Configuration SPI Pins
      - 5.1.4. Global Pins
      - 5.1.5. General I/O, LED Pins
   - 5.2. Pin Information Summary
   - 5.3. iCE40UP Part Number Description
      - 5.3.1. Tape and Reel Quantity
   - 5.4. Ordering Part Numbers
      - 5.4.1. Industrial
- Supplemental Information
- Technical Support
- Revision History
- Figure 3.1. iCE40UP5K Device, Top View Figures
- Figure 3.2. PLB Block Diagram
- Figure 3.3. PLL Diagram
- Figure 3.4. sysMEM Memory Primitives
- Figure 3.5. SPRAM Primitive
- Figure 3.6. sysDSP Functional Block Diagram (16-bit x 16-bit Multiply-Accumulate)
- Figure 3.7. sysDSP 8-bit x 8-bit Multiplier
- Figure 3.8. DSP 16-bit x 16-bit Multiplier
- Figure 3.9. I/O Bank and Programmable I/O Cell
- Figure 3.10. iCE I/O Register Block Diagram
- Figure 4.1. Power Up Sequence with SPE_VCCIO1 and VPP_2V5 Not Connected Together
- Figure 4.2. Power Up Sequence with All Supplies Connected Together to 1.8 V
- Figure 4.3. Output Test Load, LVCMOS Standards
- Table 2.1. iCE40 UltraPlus Family Selection Guide Tables
- Table 3.1. Logic Cell Signal Descriptions
- Table 3.2. Global Buffer (GBUF) Connections to Programmable Logic Blocks
- Table 3.3. PLL Signal Descriptions
- Table 3.4. sysMEM Block Configurations
- Table 3.5. EBR Signal Descriptions
- Table 3.6. SPRAM Signal Descriptions
- Table 3.7. Output Block Port Description
- Table 3.8. PIO Signal List
- Table 3.9. Supported Input Standards
- Table 3.10. Supported Output Standards
- Table 3.11. iCE40 UltraPlus Power Saving Features Description
- Table 4.1. Absolute Maximum Ratings
- Table 4.2. Recommended Operating Conditions
- Table 4.3. Power Supply Ramp Rates
- Table 4.4. Power-On-Reset Voltage Levels
- Table 4.5. DC Electrical Characteristics
- Table 4.6. Supply Current
- Table 4.7. User I^2 C Specifications
- Table 4.8. I^2 C 50 ns Delay
- Table 4.9. I^2 C 50 ns Filter
- Table 4.10. User SPI Specifications
- Table 4.11. Internal Oscillators (HFOSC, LFOSC)
- Table 4.12. sysI/O Recommended Operating Conditions
- Table 4.13. sysI/O Single-Ended DC Electrical Characteristics
- Table 4.14. Differential Comparator Electrical Characteristics
- Table 4.15. Pin-to-Pin Performance (LVCMOS25)...............................................................................................................
- Table 4.16. Register-to-Register Performance....................................................................................................................
- Table 4.17. sysDSP Timing
- Table 4.18. Single Port RAM Timing
- Table 4.19. Maximum sysI/O Buffer Performance
- Table 4.20. iCE40 UltraPlus Family Timing Adders
- Table 4.21. iCE40 UltraPlus External Switching Characteristics
- Table 4.22. sysCLOCK PLL Timing
- Table 4.23. SPI Master or NVCM Configuration Time
- Table 4.24. sysCONFIG Port Timing Specifications
- Table 4.25. RGB LED
- Table 4.26. Test Fixture Required Components, Non-Terminated Interfaces


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Acronyms in This Document Contents

A list of acronyms used in this document.

```
Acronym Definition
DFF D-style Flip-Flop
DSP Digital Signal Processor
EBR Embedded Block RAM
HFOSC High Frequency Oscillator
I^2 C Inter-Integrated Circuit
LFOSC Low Frequency Oscillator
LUT Look Up Table
LVCMOS Low-Voltage Complementary Metal Oxide Semiconductor
NVCM Non Volatile Configuration Memory
PCLK Primary Clock
PFU Programmable Functional Unit
PIC Programmable I/O Cells
PLB Programmable Logic Blocks
PLL Phase Locked Loops
SoC System on a Chip
SPI Serial Peripheral Interface
SPR Single Port RAM
WLCSP Wafer Level Chip Scale Packaging
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## 1. General Description

iCE40 UltraPlus™ family from Lattice Semiconductor is

an ultra-low power FPGA and sensor manager
designed for ultra-low power mobile applications, such
as smartphones, tablets and hand-held devices. iCE
UltraPlus is compatible with Lattice's iCE40 Ultra family
devices, containing all the functions iCE40 Ultra family

has except the high current IR LED driver. In addition,
the iCE40 UltraPlus features an additional 1 Mb SRAM,
additional DSP blocks, with additional LUTs, all which
can be used to support an always-on Voice Recognition

function in the mobile devices, without the need to
keep the higher power consuming voice codec on all
the time.

The iCE40 UltraPlus family includes integrated SPI and

I^2 C blocks to interface with virtually all mobile sensors
and application processors. In addition, the iCE
UltraPlus family also features two I/O pins that can
support the interface to I3C devices. There are two
on-chip oscillators, 10 kHz and 48 MHz, the LFOSC

(10 kHz) is ideal for low power function in always-on
applications, while HFOSC (48 MHz) can be used for
awaken activities.

The iCE40 UltraPlus family also features DSP functional
block to off-load Application Processor to pre-process

information sent from the mobile device, such as voice
data. The RGB PWM IP, with the three 24 mA constant
current RGB outputs on the iCE40 UltraPlus provides
all the necessary logic to directly drive the service LED,
without the need of external MOSFET or buffer.

The iCE40 UltraPlus family of devices are targeting for
mobile applications to perform all the functions in
iCE40 Ultra devices, such as Service LED, GPIO
Expander, SDIO Level Shift, and other custom
functions. In addition, the iCE40 UltraPlus family

devices are also targeting for Voice Recognition
application.

The iCE40 UltraPlus family features two device
densities, 2800 to 5280 Look Up Tables (LUTs) of logic

with programmable I/Os that can be used as either
SPI/I^2 C interface ports or general purpose I/O’s. Two of
the iCE40 UltraPlus I/Os can be used to interface to
higher performance I3C. It also has up to 120 kb of
Block RAMs, plus 1024 kb of Single Port SRAMs to work

with user logic.

### 1.1. Features

```
 Flexible Logic Architecture
 Two devices with 2800 to 5280 LUTs
 Offered in WLCS and QFN packages
 Ultra-low Power Devices
 Advanced 40 nm low power process
 As low as 100 μA standby current typical
 Embedded Memory
 Up to 1024 kb Single Port SRAM
 Up to 120 kb sysMEM™ Embedded Block RAM
 Two Hardened I^2 C Interfaces
 Two I/O pins to support I3C interface
 Two Hardened SPI Interfaces
 Two On-Chip Oscillators
 Low Frequency Oscillator – 10 kHz
 High Frequency Oscillator – 48 MHz
 24 mA Current Drive RGB LED Outputs
 Three drive outputs in each device
 User selectable sink current up to 24 mA
 On-chip DSP
 Signed and unsigned 8-bit or 16-bit functions
 Functions include Multiplier, Accumulator, and
Multiply-Accumulate (MAC)
 Flexible On-Chip Clocking
 Eight low skew global signal resource, six can
be directly driven from external pins
 One PLL with dynamic interface per device
 Flexible Device Configuration
 SRAM is configured through:
 Standard SPI Interface
 Internal Nonvolatile Configuration Memory
(NVCM)
 Ultra-Small Form Factor
 As small as 2.1 1 mm × 2.5 4 mm
 Applications
 Always-On Voice Recognition Application
 Smartphones
 Tablets and Consumer Handheld Devices
 Handheld Commercial and Industrial Devices
 Multi Sensor Management Applications
 Sensor Pre-processing and Sensor Fusion
 Always-On Sensor Applications
 USB 3.1 Type C Cable Detect / Power Delivery
Applications
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## 2. Product Family

Table 2. 1 lists device information and packages of the iCE40 UltraPlus family.

**Table 2. 1. iCE40 UltraPlus Family Selection Guide**

```
Part Number iCE40UP3K iCE40UP5K
Logic Cells (LUT + Flip-Flop) 2800 5280
EBR Memory Blocks 20 30
EBR Memory Bits (Kbits) 80 120
SPRAM Memory Blocks 4 4
SPRAM Memory Bits (Kbits) 1024 1024
NVCM Yes Yes
PLL 1 1
DSP Blocks (MULT16 with 32-bit Accumulator 4 8
Hardened I^2 C, SPI 2, 2 2, 2
HF Oscillator (48 MHz) 1 1
LF Oscillator (10 KHz) 1 1
24 mA LED Sink 3 3
PWM IP Block Yes Yes
Packages, ball pitch, dimension Total User I/O Count
30 - ball WLCSP, 0.4 mm, 2.1 1 mm × 2.5 4 mm 21 21
48 - ball QFN, 0.5 mm, 7.0 mm × 7.0 mm - 39
```
### 2.1. Overview

The iCE40 UltraPlus family of ultra-low power FPGAs has three devices with densities ranging from 2800 to 5280 Look-
Up Tables (LUTs) fabricated in a 40 nm Low Power CMOS process. In addition to LUT-based, low-cost programmable
logic, these devices also feature Embedded Block RAM (EBR), Single Port RAM (SPRAM), on-chip Oscillators (LFOSC,
HFOSC), two hardened I^2 C Controllers, two hardened SPI Controllers, PWM IP, three 24 mA RGB LED open-drain

drivers, I3C interface pins, and DSP blocks. These features allow the devices to be used in low-cost, high-volume
consumer and mobile applications.

The iCE40 UltraPlus FPGAs are available in very small form factor packages, as small as 2.1 1 mm × 2.5 4 mm. The small
form factor allows the device to easily fit into a lot of mobile applications, where space can be limited. Table 2. 1 lists
the LUT densities, package and I/O pin count.

The iCE40 UltraPlus devices offer I/O features such as pull-up resistors. Pull-up features are controllable on a “per pin”
basis. In addition, the iCE40 UltraPlus devices offer two I/Os with dynamic control on the pull-up resistors to support
I3C interface.

The RGB PWM IP in the iCE40 UltraPlus devices provides controls for driving the 24 mA LED Sink driver, including color

controls, LED ON/OFF time, and breathe rate.

The iCE40 UltraPlus devices also provide flexible, reliable and secure configuration from on-chip NVCM. These devices
can also configure themselves from external SPI Flash, or be configured by an external master such as a CPU.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
Lattice provides a variety of design tools that allow complex designs to be efficiently implemented using the iCE

UltraPlus family of devices. Popular logic synthesis tools provide synthesis library support for iCE40 UltraPlus. Lattice
design tools use the synthesis tool output along with the user-specified preferences and constraints to place and route
the design in the iCE40 UltraPlus device. These tools extract the timing from the routing and back-annotate it into the
design for timing verification.

Lattice provides many pre-engineered IP (Intellectual Property) modules, including a number of reference designs,
licensed free of charge, optimized for the iCE40 UltraPlus FPGA family. Lattice also can provide fully verified bitstream
for some of the widely used target functions in mobile device applications, such as ultra-low power sensor
management, gesture recognition, IR remote, barcode emulator functions. Users can use these functions as offered by
Lattice, or they can use the design to create their own unique required functions. For more information regarding

Lattice's reference designs or fully-verified bitstreams, contact your local Lattice representative.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## 3. Architecture

### 3.1. Architecture Overview

The iCE40 UltraPlus family architecture contains an array of Programmable Logic Blocks (PLB), two Oscillator
Generators, two user configurable I^2 C controllers, two user configurable SPI controllers, blocks of sysMEM™ Embedded
Block RAM (EBR) and Single Port RAM (SPRAM) surrounded by Programmable I/O (PIO). Figure 3. 1 shows the block
diagram of the iCE40UP5K device.

I/O Bank 0

I/O Bank 2

**I**^2 **C I**^2 C

**SPI SPI**

##### HFOSC LFOSC

```
config
```
```
DSP
```
```
NVCM
```
```
Flip-flop with Enableand Reset Controls
```
```
Carry Logic
4-Input Look-up
Table (LUT)
```
```
8 Logic Cells = Programmable Logic Block
```
##### PLB

```
RGB I/ORGB I/ORGB I/O I3C I/OI3C I/O
```
```
PLL
```
```
5 4 Kb DPRAM 5 4 Kb DPRAM
```
```
5 4 Kb DPRAM 5 4 Kb DPRAM
```
```
5 4 Kb DPRAM 5 4 Kb DPRAM
```
I/O Bank 1_SPI

```
config
```
```
DSP
```
```
DSP
```
```
DSP
```
```
DSP
```
```
DSP
```
```
DSP
```
```
DSP SPRAM256 Kb
```
```
PWM IP
```
```
5 PLB Rows
```
```
50 ns Filter
```
```
50 ns Filter
```
```
50 ns Delay
```
```
50 ns Delay
```
```
256 Kb
SPRAM
```
```
256 Kb
SPRAM
```
```
256 Kb
SPRAM
```
```
Figure 3. 1. iCE40UP5K Device, Top View
```
The Programmable Logic Blocks (PLB) and sysMEM EBR blocks, are arranged in a two-dimensional grid with rows and

columns. Each column has either PLB or EBR blocks. The PIO cells are located at the top and bottom of the device,
arranged in banks. The PLB contains the building blocks for logic, arithmetic, and register functions. The PIOs utilize a
flexible I/O buffer referred to as a sysI/O buffer that supports operation with a variety of interface standards. The
blocks are connected with many vertical and horizontal routing channel resources. The place and route software tool

automatically allocates these routing resources.

In the iCE40 UltraPlus family, there are three sysI/O banks, one on top and two at the bottom. User can connect some
VCCIOs together, if all the I/Os are using the same voltage standard. See the Power-up Supply Sequence section. The
sysMEM EBRs are large 4 kb, dedicated fast memory blocks. These blocks can be configured as RAM, ROM or FIFO with
user logic using PLBs.

In addition to the EBR, the iCE40 UltraPlus devices also feature four 256 kb SPRAM blocks that can be cascaded to
create up to 1 Mb block. It is useful for temporary storage of large quantities of information.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
Every device in the family has two user SPI ports, one of these (right side) SPI ports also supports programming and

configuration of the device. The iCE40 UltraPlus also includes two user I^2 C ports, two oscillators, and high current RGB
LED sink.

#### 3.1.1. PLB Blocks

The core of the iCE40 UltraPlus device consists of Programmable Logic Blocks (PLB) which can be programmed to
perform logic and arithmetic functions. Each PLB consists of eight interconnected Logic Cells (LC) as shown in Figure
3. 2. Each LC contains one LUT and one register.

```
= Statically defined by configuration program
```
##### LUT

```
Carry Logic
```
```
Logic Cell
```
```
SR
```
```
EN
```
```
D Q
```
##### DFF

```
Flip-flop with
optional enable and
set or reset controls
```
```
Four-input
Look-Up Table
(LUT)
```
```
Clock
```
```
Enable
FCOUT
```
```
FCIN
```
```
Set/Reset
```
```
Shared Block-Level Controls
```
```
Programmable
Logic Block (PLB)
```
```
8 Logic Cells (LCs)
```
```
I
I
I
I
```
```
O
```
```
1
```
```
0
```
## Figure 3.2. PLB Block Diagram

**Logic Cells**

Each Logic Cell includes three primary logic elements shown in Figure 3. 2.

 A four-input Look-Up Table (LUT) builds any combinational logic function, of any complexity, requiring up to four
inputs. Similarly, the LUT element behaves as a 16x1 Read-Only Memory (ROM). Combine and cascade multiple
LUTs to create wider logic functions.

 A ‘D’-style Flip-Flop (DFF), with an optional clock-enable and reset control input, builds sequential logic functions.
Each DFF also connects to a global reset signal that is automatically asserted immediately following device
configuration.

 Carry Logic boosts the logic efficiency and performance of arithmetic functions, including adders, subtracters,
comparators, binary counters and some wide, cascaded logic functions.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
Table 3. 1 lists the logic cell signals.

## Table 3.1. Logic Cell Signal Descriptions

```
Function Type Signal Name Description
Input Data signal I0, I1, I2, I3 Inputs to LUT
Input Control signal Enable Clock enable shared by all LCs in the PLB
Input Control signal Set/Reset* Asynchronous or synchronous local set/reset shared by
Input Control signal Clock all LCs in the PLB.Clock one of the eight Global Buffers, or from^ the
general-purpose interconnects fabric shared by all LCs
in the PLB
```
```
Input Inter-PLB signal FCIN Fast carry in
Output Data signals O LUT or registered output
Output Inter-PFU signal FCOUT Fast carry out
```
*Note: If Set/Reset is not used, then the flip-flop is never set/reset, except when cleared immediately after configuration.

#### 3.1.2. Routing

There are many resources provided in the iCE40 UltraPlus devices to route signals individually with related control
signals. The routing resources consist of switching circuitry, buffers and metal interconnect (routing) segments.

The inter-PLB connections are made with three different types of routing resources: Adjacent (spans two PLBs), x
(spans five PLBs) and x12 (spans thirteen PLBs). The Adjacent, x4 and x12 connections provide fast and efficient
connections in the diagonal, horizontal and vertical directions.

The design tool takes the output of the synthesis tool and places and routes the design.

#### 3.1.3. Clock/Control Distribution Network

Each iCE40 UltraPlus device has six global inputs, two pins on the top bank and four pins on the bottom bank.

These global inputs can be used as high fanout nets, clock, reset or enable signals. The dedicated global pins are
identified as Gxx and each drives one of the eight global buffers. The global buffers are identified as GBUF[7:0]. These

six inputs may be used as general purpose I/O if they are not used to drive the clock nets.

Table 3. 2 lists the connections between a specific global buffer and the inputs on a PLB. All global buffers optionally
connect to the PLB CLK input. Any four of the eight global buffers can drive logic inputs to a PLB. Even-numbered global
buffers optionally drive the Set/Reset input to a PLB. Similarly, odd-numbered buffers optionally drive the PLB clock-
enable input. GBUF[7:6, 3:0] can connect directly to G[7:6, 3:0] pins respectively. GBUF4 and GBUF5 can connect to the

two on-chip Oscillator Generators (GBUF4 connects to LFOSC, GBUF5 connects to HFOSC).

## Table 3.2. Global Buffer (GBUF) Connections to Programmable Logic Blocks

```
Global Buffer LUT Inputs Clock Reset Clock Enable
GBUF
```
```
Yes, any 4 of 8 GBUF
Inputs
```
#####   —

##### GBUF1  — 

##### GBUF2   —

##### GBUF3  — 

##### GBUF4   —

##### GBUF5  — 

##### GBUF6   —

##### GBUF7  — 


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
The maximum frequency for the global buffers are listed in Table 4. 21.

**Global Hi-Z Control**

The global high-impedance control signal, GHIZ, connects to all I/O pins on the iCE40 UltraPlus device. This GHIZ signal
is automatically asserted throughout the configuration process, forcing all user I/O pins into their high-impedance
state.

**Global Reset Control**

The global reset control signal connects to all PLB and PIO flip-flops on the iCE40 UltraPlus device. The global reset
signal is automatically asserted throughout the configuration process, forcing all flip-flops to their defined wake-up
state. For PLB flip-flops, the wake-up state is always reset, regardless of the PLB flip-flop primitive used in the
application.

#### 3.1.4. sysCLOCK Phase Locked Loops (PLLs)

The sysCLOCK PLLs provide the ability to synthesize clock frequencies. The iCE40 UltraPlus devices have one sysCLOCK
PLL. REFERENCECLK is the reference frequency input to the PLL and its source can come from an external I/O pin, the

internal Oscillator Generators from internal routing. EXTFEEDBACK is the feedback signal to the PLL which can come
from internal routing or an external I/O pin. The feedback divider is used to multiply the reference frequency and thus
synthesize a higher frequency clock output.

The PLLOUT output has an output divider, thus allowing the PLL to generate different frequencies for each output. The
output divider can have a value from 1 to 64 (in increments of 2X). The PLLOUT outputs can all be used to drive the

iCE40 UltraPlus global clock network directly or general purpose routing resources can be used.

The LOCK signal is asserted when the PLL determines it has achieved lock and de-asserted if a loss of lock is detected. A
block diagram of the PLL is shown in Figure 3. 3.

The timing of the device registers can be optimized by programming a phase shift into the PLLOUT output clock which

will advance or delay the output clock with reference to the REFERENCECLK clock. This phase shift can be either

programmed during configuration or can be adjusted dynamically. In dynamic mode, the PLL may lose lock after a phase

adjustment on the output used as the feedback source and not relock until the tLOCK parameter has been satisfied.

For more details, refer to iCE40 sysCLOCK PLL Design and Usage Guide (FPGA-TN- 02052 ).

```
In put
Divider
```
```
DIVR
Lo w-Pass
Filter
```
```
Vol tage
Control led
Osc ill ator (VCO)
DividerVCO
```
```
DIVQ
```
```
Feed bac k
Divider
```
```
DIVF
```
```
RANGE
```
```
Phase
Detec tor
```
```
Feed bac k_Path
```
```
Fine Delay
Adj ustment
Feed bac k
Shifter Phase
```
```
LATCHINPUTVALUE
```
```
REFEREN CE CLK
```
```
DYNAMICDELAY[7:0]
```
```
BYPAS S
```
```
RESET
```
```
EXTFE EDBACK
```
```
PLLOUTCORE
```
```
PLLOUTG LOBAL
```
```
LOCK
```
```
BYPAS S
```
```
Lo w Power mode
(iCEgate enabled)
```
```
SIMPLE
```
```
EXTERNAL
```
```
GNDPLL VCCPL L
```
```
Fine Delay
Adj ustment
Output Port
```
## Figure 3.3. PLL Diagram

Table 3. 3 provides signal descriptions of the PLL block.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Table 3.3. PLL Signal Descriptions

```
Signal Name Direction Description
REFERENCECLK Input Input reference clock
BYPASS Input The BYPASS control selects which clock signal connects to the PLLOUT output.
0 – PLL generated signal
1 – REFERENCECLK
EXTFEEDBACK Input External feedback input to PLL. Enabled when the FEEDBACK_PATH attribute is set
to EXTERNAL.
DYNAMICDELAY[7:0] Input Fine delay adjustment control inputs. Enabled when DELAY_ADJUSTMENT_MODE
is set to DYNAMIC.
LATCHINPUTVALUE Input When enabled, puts the PLL into low-power mode; PLL output is held static at the
last input clock value. Set ENABLE ICEGATE_PORTA and PORTB to ‘1’ to enable.
PLLOUTGLOBAL Output Output from the Phase-Locked Loop (PLL). Drives a global clock network on the
FPGA. The port has optimal connections to global clock buffers GBUF4 and GBUF5.
PLLOUTCORE Output Output clock generated by the PLL, drives regular FPGA routing. The frequency
generated on this output is the same as the frequency of the clock signal generated
on the PLLOUTLGOBAL port.
LOCK Output When High, indicates that the PLL output is phase aligned or locked to the input
reference clock.
RESET Input Active low reset.
SCLK Input Input, Serial Clock used for re-programming PLL settings.
SDI Input Input, Serial Data used for re-programming PLL settings.
```
#### 3.1.5. sysMEM Embedded Block RAM Memory

Larger iCE40 UltraPlus device includes multiple high-speed synchronous sysMEM Embedded Block RAMs (EBRs), each
4 kbit in size. This memory can be used for a wide variety of purposes including data buffering and FIFO.

**sysMEM Memory Block**

The sysMEM block can implement single port, pseudo dual port, or FIFO memories with programmable logic resources.
Each block can be used in a variety of depths and widths as listed in Table 3. 4.

## Table 3.4. sysMEM Block Configurations

```
Block RAM
Configuration
```
```
Block RAM
Configuration
and Size
```
```
WADDR Port
Size (Bits)
```
```
WDATA Port
Size (Bits)
```
```
RADDR Port
Size (Bits)
```
```
RDATA Port
Size (Bits)
```
```
MASK Port
Size (Bits)
```
```
SB_RAM256x
SB_RAM256x16NR
SB_RAM256x16NW
SB_RAM256x16NRNW
```
```
256x16 (4 k) 8 [7:0] 16 [15:0] 8 [7:0] 16 [15:0] 16 [15:0]
```
```
SB_RAM512x
SB_RAM512x8NR
SB_RAM512x8NW
SB_RAM512x8NRNW
```
```
512x8 (4 k) 9 [8:0] 8 [7:0] 9 [8:0] 8 [7:0] No Mask Port
```
```
SB_RAM1024x
SB_RAM1024x4NR
SB_RAM1024x4NW
SB_RAM1024x4NRNW
```
```
1024x4 (4 k) 10 [9:0] 4 [3:0] 10 [9:0] 4 [3:0] No Mask Port
```
```
SB_RAM2048x
SB_RAM2048x2NR
SB_RAM2048x2NW
SB_RAM2048x2NRNW
```
```
2048x2 (4 k) 11 [10:0] 2 [1:0] 11 [10:0] 2 [1:0] No Mask Port
```
**Note** : For iCE40 UltraPlus, the primitive name without “Nxx” uses rising-edge Read and Write clocks. “NR” uses rising-edge Write
clock and falling-edge Read clock. “NW” uses falling-edge Write clock and rising-edge Read clock. “NRNW” uses failing-edge clocks
on both Read and Write.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
**RAM Initialization and ROM Operation**

If desired, the contents of the RAM can be pre-loaded during device configuration.

By preloading the RAM block during the chip configuration cycle and disabling the write controls, the sysMEM block
can also be utilized as a ROM.

**Memory Cascading**

Larger and deeper blocks of RAM can be created using multiple EBR sysMEM Blocks.

**RAM4k Block**

Figure 3. 4 shows the 256x16 memory configurations and their input/output names. In all the sysMEM RAM modes, the
input data and addresses for the ports are registered at the input of the memory array.

##### WCLK

##### WE RE

##### WCLKE RCLKE

##### RCLK

##### WDATA[15:0] RDATA[15:0]

##### MASK[15:0]

##### WADDR[7:0] RADDR[7:0]

```
Write Port Read Port
```
##### RAM4K

```
RAM Block
(256x16)
```
## Figure 3.4. sysMEM Memory Primitives

Table 3. 5 lists the EBR signals.

## Table 3.5. EBR Signal Descriptions

```
Signal Name Direction Description
WDATA[15:0] Input Write Data input.
MASK[15:0] Input Masks write operations for individual data bit-lines.
0 – Write bit
1 – Do not write bit
WADDR[7:0] Input Write Address input. Selects one of 256 possible RAM locations.
WE Input Write Enable input.
WCLKE Input Write Clock Enable input.
WCLK Input Write Clock input. Default rising-edge, but with falling-edge option.
RDATA[15:0] Output Read Data output.
RADDR[7:0] Input Read Address input. Selects one of 256 possible RAM locations.
RE Input Read Enable input.
RCLKE Input Read Clock Enable input.
RCLK Input Read Clock input. Default rising-edge, but with falling-edge option.
```
For further information on the sysMEM EBR block, refer to Memory Usage Guide for iCE40 Devices (FPGA-TN- 02002 ).


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 3.1.6. sysMEM Single Port RAM Memory (SPRAM)

The SPRAM block is implemented to be accessed only as single port. Each block of SPRAM is designed to be 16K x 16
(256 kbits) in size. See Figure 3. 5.

**SPRAM Data Width**

The SPRAM is designed with fixed 16-bit data width. However, the block contains nibble mask control on the write
input that allows the user logic to operate the SPRAM as x4 or x8 with this control on the write side, and user logic to
select which nibble/byte in the read side.

**SPRAM Initialization and ROM Operation**

There is no pre-load into the SPRAM during device configuration, therefore, the SPRAM is not initialized after
configuration.

**SPRAM Cascading**

Deeper SPRAM can be created using multiple SPRAM blocks, up to four blocks (64K x 16)

**SPRAM Power Modes**

There are three power modes in the SPRAM that the users can select during normal operation. This reduces the SPRAM
block power when it Is not needed, allow lower power consumption in an always-on application. These modes are:

 **Standby Mode** : SPRAM stops all activity, and SPRAM freezes in its current state. Memory contents are retained,
memory outputs are retained, and all register contents are retained.

 **Sleep Mode** : SPRAM block is shut down on all peripheral circuit, except the memory core. Memory contents are
retained, memory outputs and register contents are clear and become unknown.
 **Power Off Mode** : Power source to the SPRAM is disconnected. This is the lowest power state. Memory contents

```
are lost. Memory outputs are unknown.
```
```
DATAOUT [15:0]
```
```
Single Port RAM Primitive
SB_SPRAM256KA
```
```
MASKWREN [3:0]
```
```
WREN
```
```
CHIPSELECT
```
```
CLOCK
```
```
STANDBY
```
```
SLEEP
```
```
POWEROFFN
```
```
MASKWREN [3:0]
```
```
WREN
```
## Figure 3.5. SPRAM Primitive


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Table 3.6. SPRAM Signal Descriptions

```
Signal Name Direction Description
ADDRESS[13:0] Input Address input
DATAIN[15:0] Input Write Data input
MASKWREN[3:0] Input Nibble WE control
WREN Input Write Enable
CHIPSELECT Input Enable SPRAM
CLOCK Input Clock input
STANDY Input Standby Mode
SLEEP Input Sleep Mode
POWEROFF Input Switch off power source to SPRAM
DATAOUT[15:0] Output Output Data
```
For further information on sysMEM SPRAM block, refer to iCE40 SPRAM Usage Guide (FPGA-TN- 02022 ).

#### 3.1.7. sysDSP

The iCE40 UltraPlus family provides an efficient sysDSP architecture that is very suitable for low-cost Digital Signal
Processing (DSP) functions for mobile applications. Typical functions used in these applications are Multiply,

Accumulate, and Multiply-Accumulate. The block can also be used for simple Add and Subtract functions.

**iCE40 UltraPlus sysDSP Architecture Features**

The iCE40 UltraPlus sysDSP supports many functions that include the following:

 Single 16-bit x 16-bit Multiplier, or two independent 8-bit x 8-bit Multipliers
 Optional independent pipeline control on Input Register, Output Register, and Intermediate Reg faster clock

performance
 Single 32-bit Accumulator, or two independent 16-bit Accumulators

 Single 32-bit, or two independent 16-bit Adder/Subtracter functions, registered or asynchronous

 Cascadable to create wider Accumulator blocks

Figure 3. 6 shows the block diagram of the sysDSP block. The block consists of the Multiplier section with a bypassable
Output register, Input Register, and Intermediate register between Multiplier and AC timing to achieve the highest

performance.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
HLD
R
```
```
DQ
```
```
0
A [15 :0]^1
C 1
```
```
HLD
R
```
```
DQ
```
```
0
B [15 :0]^1
C 2
```
```
R
```
```
DQ
```
```
0
1
C 4
```
```
R
```
```
DQ
```
```
0
1
C 6
```
```
A [15 :8]
B [15 :8]
```
```
A [7:0]
B [15 :8]
```
```
A [15 :8]
B [7:0]
```
```
A [7:0]
B [7:0]
```
```
+
```
```
+
```
```
+
```
```
P [7:0]
```
```
P [31 :24 ]
```
```
P [15 :8]
```
```
P [23 : 16 ]
```
```
R
```
```
DQ
```
```
0
1
8x 8 C 5
```
```
8x 8
```
```
8x 8
```
```
8x 8
```
```
0
1
R C 7
```
```
DQ
```
```
[7:0]
```
```
[15 : 8]
```
```
[7:0]
```
```
[15 :8]
```
```
[7:0]
```
```
[15 : 0]
```
```
[7:0]
[15 : 0]
```
```
[15 :0]
```
```
[15 :0]
```
```
[15 : 0]
```
```
[15 :0]
```
```
HLD
R
```
```
DQ
```
```
0
C [15 :0] 1
C 0
```
```
HLD
R
```
```
DQ
```
```
0
D [15 :0]^1
C 3
```
```
0
```
(^1) HLD
R
DQ
0
1
2
C 10
C 11
0
1
2
3
C 8C 9
±
Q [31 :16 ]
C 12
0
1
O [31 : 16 ]
0
(^1) HLD
R
DQ
0
1
2
C 17
C 18
0
1
2
3
C 15C 16
±
Q [15 :
C 19
0
1
8x 8= 16 O [15 : 0]
8x 8= 16
16 x 16 = 32 [31 : 16 ] 012 3
C 14
C 13
012 3 C 21C 20
01
01
CICAS
CHLD
IHRST
CO
OHHLDOHRST
CI
OHLDA
OLLDAOLHLD
OLRST
Multiplier Accumulator
Hi
Lo
Input Registers
16 x 16 Pipeline
Registers
16 x 16
Pipeline
Register
ILRST
AHLD
BHLD
DHLD
OHADS
OLADS
CLK
ENA
W
X
Y
Z
F
G
H
C 228x 8 PowerSave
R
DQ
0
1
C 6
J
K
L
ASGND=C
BSGND=C
P Q
R S
C
A
B
D
COCAS
LCO
LCOCAS
HLD
HLD
HLD
3
3
SIGNEXTIN
SIGNEXTOUT
X [15 ]
Z [15 ]
LCI
HCI
01
01
CSA
CSA
[15 :8]
[15 :8]

## Figure 3.6. sysDSP Functional Block Diagram (16-bit x 16-bit Multiply-Accumulate)

## Table 3.7. Output Block Port Description

```
Signal Primitive^
Port Name
```
```
Width
```
```
Input/
Output
```
```
Function Default
```
```
CLK CLK 1 Input Clock Input. Applies to all clocked elements in the
sysDSP block
```
##### —

```
ENA CE 1 Input Clock Enable Input. Applies to all clocked elements
in the sysDSP block.
0 – Not enabled
1 – Enabled
```
```
0 – Enabled
```
A[15:0] A[15:0] 16 Input (^) Input to the A Register. Feeds the Multiplier or is a
direct input to the Adder Accumulator
16'b
B[15:0] B[15:0] 16 Input Input to the B Register. Feeds the Multiplier or is a
direct input to the Adder Accumulator
16'b
C[15:0] C[15:0] 16 Input Input to the C Register. It is a direct input to the
Adder Accumulator
16'b
D[15:0] D[15:0] 16 Input Input to the D Register. It is a direct input to the
Adder Accumulator
16'b
AHLD AHOLD 1 Input A Register Hold.
0 – Update
1 – Hold
0 – Update
BHLD BHOLD 1 Input B Register Hold.
0 – Update
1 – Hold
0 – Update


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
Signal Primitive^
Port Name
```
```
Width
```
```
Input/
Output
```
```
Function Default
```
```
CHLD CHOLD 1 Input C Register Hold.
0 – Update
1 – Hold
```
```
0 – Update
```
```
DHLD DHOLD 1 Input D Register Hold.
0 – Update
1 – Hold
```
```
0 – Update
```
```
IHRST IRSTTOP 1 Input Reset input to A and C input registers, and the
pipeline registers in the upper half of the Multiplier
Section.
0 – No reset
1 – Reset
```
```
0 – No reset
```
```
ILRST IRSTBOT 1 Input Reset input to B and D input registers, and the
pipeline registers in the lower half of the Multiplier
Section. It also resets the Multiplier result pipeline
register.
0 – No reset
1 – Reset
```
```
0 – No reset
```
```
O[31:0] O[31:0] 32 Output Output of the sysDSP block. This output can be:
 O[31:0] – 32 - bit result of 16x16 Multiplier or
MAC
 O[31:16] – 16 - bit result of 8x8 upper half
Multiplier or MAC
O[15:0] – 16 - bit result of 8x8 lower half Multiplier
or MAC
```
##### —

```
OHHLD OHOLDTOP 1 Input High-order (upper half) Accumulator Register Hold.
0 – Update
1 – Hold
```
```
0 – Update
```
```
OHRST ORSTTOP 1 Input Reset input to high-order (upper half) bits of the
Accumulator Register.
0 – No reset
1 – Reset
```
```
0 – No reset
```
```
OHLDA OLOADTOP 1 Input High-order (upper half) Accumulator Register
Accumulate/Load control.
0 – Accumulate, register is loaded with
Adder/Subtracter results
1 – Load, register is loaded with Input C or C
Register
```
##### 0 –

```
Accumulate
```
```
OHADS ADDSUBTOP 1 Input High-order (upper half) Accumulator Add or
Subtract select.
0 – Add
1 – Subtract
```
```
0 – Add
```
```
OLHLD OHOLDBOT 1 Input Low-order (lower half) Accumulator Register Hold.
0 – Update
1 – Hold
```
```
0 – Update
```
```
OLRST ORSTBOT 1 Input Reset input to Low-order (lower half) bits of the
Accumulator Register.
0 – No reset
1 – Reset
```
```
0 – No reset
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
Signal Primitive^
Port Name
```
```
Width
```
```
Input/
Output
```
```
Function Default
```
```
OLLDA OLOADBOT 1 Input Low-order (lower half) Accumulator Register
Accumulate/Load control.
0 – Accumulate, register is loaded with
Adder/Subtracter results
1 – Load, register is loaded with Input C or C
Register
```
##### 0 –

```
Accumulate
```
```
OLADS ADDSUBBOT 1 Input Low-order (lower half) Accumulator Add or
Subtract select.
0 – Add
1 – Subtract
```
```
0 – Add
```
```
CICAS ACCUMCI 1 Input Cascade Carry/Borrow input from previous sysDSP
block
```
##### —

```
CI CI 1 Input Carry/Borrow input from lower logic tile —
COCAS ACCUMCO 1 Output Cascade Carry/Borrow output to next sysDSP block —
CO CO 1 Output Carry/Borrow output to higher logic tile —
SIGNEXTIN SIGNEXTIN 1 Input Sign extension input from previous sysDSP block —
SIGNEXTOUT SIGNEXTOUT 1 Output Sing extension output to next sysDSP block —
```
The iCE40 UltraPlus sysDSP can support the following functions:

 8 - bit x 8-bit Multiplier
 16 - bit x 16-bit Multiplier

 16 - bit Adder/Subtracter

 32 - bit Adder/Subtracter

 16 - bit Accumulator
 32 - bit Accumulator

 8 - bit x 8-bit Multiply-Accumulate

 16 - bit x 16-bit Multiply-Accumulate

Figure 3. 7 shows the path for an 8-bit x 8-bit Multiplier using the upper half of sysDSP block.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
HLD
R
```
```
D Q
```
```
0
A [ 15 :0 ]^1
C 1
```
```
HLD
R
```
```
D Q
```
```
0
B [ 15 :0 ]^1
C 2
```
```
R
```
```
D Q
```
```
0
1
C 4
```
```
R
```
```
D Q
```
```
0
1
C 6
```
```
A [ 15 :8 ]
B [ 15 :8 ]
```
```
A [ 7 :0 ]
B [ 15 :8 ]
```
```
A [ 15 :8 ]
B [ 7 :0 ]
```
```
A [ 7 :0 ]
B [ 7 :0 ]
```
```
+
```
```
+
```
```
+
```
```
P [ 7: 0 ]
```
```
P [ 31 : 24 ]
```
```
P [ 15 : 8 ]
```
```
P [ 23 : 16 ]
```
```
R
```
```
D Q
```
```
0
1
8 x 8 C 5
```
```
8 x 8
```
```
8 x 8
```
```
8 x 8
```
```
0
1
R C 7
```
```
D Q
```
```
[ 15 :8 ]
```
```
[15:8]
```
```
[15:8]
```
```
[ 15 :0 ]
```
```
[15 :0 ]
```
```
[ 15 : 0 ]
```
```
[ 15 : 0 ]
```
```
[ 15 : 0 ]
```
```
[ 15 :0 ]
```
```
HLD
R
```
```
D Q
```
```
0
C[ 15 :0 ] 1
C 0
```
```
HLD
R
```
```
D Q
```
```
0
D [ 15 :0 ] 1
C 3
```
```
0
```
(^1) HLD
R
D Q
0
1
2
C 10C 11
0
1
2
3
C 8C 9
±
Q [ 31 :16 ]
C12
0
1
O [ 31 :16 ]
0
(^1) HLD
R
D Q
0
1
2
C 17C 18
0
1
2
3
C 15C 16
±
Q [15:0]
C19
0
1
8 x 8 = 16 O [ 15 :0 ]
8 x 8 =16
16 x 16 =32 [ 31 : 16 ] 0 1 2 3 C13C14
0 1 2 3 C21C20
01
01
CICAS
CHLD
IHRST
CO
OHHLDOHRST
CI
OHLDA
OLLDA
OLHLDOLRST
Multiplier Accumulator
High
Low
Input Registers
16 x 16 Pipeline
Registers
16 x 16
Pipeline
Register
( 25 - FEB - 2012 )
ILRST
AHLD
BHLD
DHLD
OHADS
OLADS
CLK
ENA
W
X
Y
Z
F
G
H
C 228 x 8 PowerSave
R
D Q
0
1
C 6
J
K
L
ASGND=C23
BSGND=C24
P Q
R S
C
A
B
D
COCAS
LCO
LCOCAS
HLD
HLD
HLD
3
3
SIGNEXTIN
SIGNEXTOUT
X [ 15 ]
Z [ 15 ]
LCI
HCI
01
01
CSA
CSA
[7:0]
[7:0]
[7:0]
[15:8]
[7:0]

## Figure 3.7. sysDSP 8-bit x 8-bit Multiplier

Figure 3. 8 shows the path for an 16-bit x 16-bit Multiplier using the upper half of sysDSP block.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
(^)
HLD
R
DQ
0
A[ 15 : 0 ]^1
C 1
(^)
HLD
R
DQ
0
B[ 15 : 0 ]^1
C 2
R
DQ
0
1
C 4
(^)
R
DQ
0
1
C 6
A[ 15 : 8 ]
B[ 15 : 8 ]
A[ 7 : 0 ]
B[ 15 : 8 ]
A[^15 :^8 ]
B[ 7 : 0 ]
A[ 7 : 0 ]
B[ 7 : 0 ]
(^)
+
+
(^) +
P[ 7 : 0 ]
P[^31 :^24 ]
P[ 15 : 8 ]
P[ 23 : 16 ]
R^
DQ^
0
1
8 x 8 C^5
8 x 8
8 x 8
8x8
0
1
R^ C^7
DQ
[ 7 : 0 ]
[ 15 : 8 ]
[ 7 : 0 ]
[ 15 : 8 ]
[ 7 : 0 ]
[ 15 :0]
[ 15 :0]
[ 15 : 0 ]
[^15 :^0 ]
[ 15 : 0 ]
[ 15 : 0 ]
HLD
R
DQ
0
C[ 15 : 0 ] 1
C 0
HLD
R
DQ
0
D[ 15 : 0 ]^1
(^)
C 3
(^01)
HLDR
DQ
0
1
2
C (^10)
C 11
0
1
2
3
C (^8)
C 9
±
Q[ 31 : 16 ]
C 12
0
1
O[ 31 : 16 ]
(^01)
HLDR
DQ
0
1
2
C 17 C^18
0
1
2
3
C 15 C^16
±
Q[ 15 : 0
C 19
0
1
O[^15 : 0 ]
8 x 8 = 16
8 x 8 = 16
16 x 16 = (^32) [ 31 : 16 ] 0123
(^) C^14
(^1) C^3
(^0123)
(^) C 20
C^21
01
01
CICAS
CHLD
IHRST
CO
OHHLDOHRST
CI
OHLDA
OLLDAOLHLD
OLRST
Multiplier Accumulator
High
Input Registers
16 x 16 Pipeline
Registers
16 x 16
Pipeline
Register
( 25 - FEB- 2012 )
ILRST
AHLD
BHLD
DHLD
OHADS
OLADS
ENACLK
W
X
Y
Z
F
G
H
C (^22) 8x8 PowerSave
R
DQ
0
1
C 6
J
K
L
P Q
R S
C
A
B
D
COCAS
LCO
LCOCAS
HLD
HLD
HLD
3
3
SIGNEXTIN
SIGNEXTOUT
X[ 15 ]
Z[ 15 ]
LCI
HCI
01
01
Low
[ 7 : 0 ]
[ 15 : 8 ]
[ 15 : 8 ]
CSA
CSA
ASGND=23
BSGND=24

## Figure 3.8. DSP 16-bit x 16-bit Multiplier

#### 3.1.8. sysI/O Buffer Banks

iCE40 UltraPlus devices have up to three I/O banks with independent VCCIO rails. The configuration SPI interface signals

are powered by SPI_VCCIO1. Refer to the Pin Information Summary table.

**Programmable I/O (PIO)**

The programmable logic associated with an I/O is called a PIO. The individual PIOs are connected to their respective
sysI/O buffers and pads. The PIOs are placed on the top and bottom of the devices.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
OUT
```
```
OE
```
```
VC C IO
I/O Bank 0 or 2
Volt age S upply
0 = Hi -Z
1 = Out put
Enabled
```
```
PAD
```
```
HD
```
```
iCE GAT E
HOLD
```
```
Disabled ‘ 0 ’
```
```
OUTCLK
```
```
OUTCLK
```
```
I NC LK
```
```
Enabled ‘ 1 ’
```
```
Lat ch inhibits
swit ching f or
powe r saving
```
```
Pull-up
Enable
```
```
Pull-up
```
```
Gxx pins optionally
connect dire ctly t o
an associate d
GBU F global
buff er
```
```
IN
```
```
5 PL B R ows
```
```
5 0 n s Filt er
```
```
5 0 n s Filt er
```
```
5 0 n s Dela y
```
```
5 0 n s Dela y
```
## Figure 3.9. I/O Bank and Programmable I/O Cell

The PIO contains three blocks: an input register block, output register block iCEGate™ and tri-state register block. To

save power, the optional iCEGate™^ latch can selectively freeze the state of individual, non-registered inputs within an
I/O bank. Note that the freeze signal is common to the bank. These blocks can operate in a variety of modes along with
the necessary clock and selection logic.

**Input Register Block**

The input register blocks for the PIOs on all edges contain registers that can be used to condition high-speed interface

signals before they are passed to the device core.

**Output Register Block**

The output register block can optionally register signals from the core of the device before they are passed to the
sysI/O buffers.

Figure 3. 10 shows the input/output register block for the PIOs.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
(1,0)
```
```
(1,0)
```
```
(1,0)
0
1
```
```
(1,0)
```
```
Pad
```
```
Pad
```
```
CLOCK_ENABLE
```
```
OUTPUT_ENABLE
```
```
OUTPUT_ENABLE
```
```
OUTPUT_CLK
INPUT_CLK
```
```
LATCH_INPUT_VALUE
```
```
LATCH_INPUT_VALUE
```
```
D_IN_1
D_IN_0
```
```
D_OUT_1
D_OUT_0
```
```
D_IN_1
D_IN_0
```
```
D_OUT_1
D_OUT_0
```
```
PIO Pair
```
```
0
1
```
```
= Statically defined by configuration program.
```
## Figure 3.10. iCE I/O Register Block Diagram

## Table 3.8. PIO Signal List

```
Pin Name I/O Type Description
OUTPUT_CLK Input Output register clock
CLOCK_ENABLE Input Clock enable
INPUT_CLK Input Input register clock
OUTPUT_ENABLE Input Output enable
D_OUT_0/1 Input Data from the core
D_IN_0/1 Output Data to the core
LATCH_INPUT_VALUE Input Latches/holds the Input Value
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 3.1.9. sysI/O Buffer

Each I/O is associated with a flexible buffer referred to as a sysI/O buffer. These buffers are arranged around the
periphery of the device in groups referred to as banks. The sysI/O buffers allow users to implement a wide variety of

standards that are found in today’s systems with LVCMOS interfaces.

**Typical I/O Behavior During Power-up**

The internal power-on-reset (POR) signal is deactivated when VCC, SPI_VCCIO1 and VPP_2V5 reach the level defined in Table
4. 4. After the POR signal is deactivated, the FPGA core logic becomes active. You must ensure that all VCCIO banks are

active with valid input logic levels to properly control the output logic states of all the I/O banks that are critical to the
application. The default configuration of the I/O pins in a device prior to configuration is tri-stated with a weak pull-up
to VCCIO. The I/O pins maintain the pre-configuration state until VCC, SPI_VCCIO1 and VPP_2V5 reach the defined levels. The

I/Os take on the software user-configured settings only after POR signal is deactivated and the device performs a
proper download/configuration. Unused I/Os are automatically blocked and the pull-up termination is disabled.

**Supported Standards**

The iCE40 UltraPlus sysI/O buffer supports both single-ended input/output standards, and used as differential
comparators. The buffer supports the LVCMOS 1.8 V, 2.5 V, and 3.3 V standards. The buffer has individually
configurable options for bus maintenance (weak pull-up or none).

Table 3. 9 and Table 3. 10 show the I/O standards (together with their supply and reference voltages) supported by the
iCE40 UltraPlus devices.

**Differential Comparators**

The iCE40 UltraPlus devices provide differential comparator on pairs of I/O pins. These comparators are useful in some
mobile applications. See the Pin Information Summary section to locate the corresponding paired I/Os with differential
comparators.

## Table 3.9. Supported Input Standards

```
I/O Standard
```
```
VCCIO (Typical)
3.3 V 2.5 V 1.8 V
Single-Ended Interfaces
LVCMOS33 Yes — —
LVCMOS25 — Yes —
LVCMOS18 — — Yes
```
## Table 3.10. Supported Output Standards

```
I/O Standard VCCIO (Typical)^
Single-Ended Interfaces
LVCMOS33 3.3 V
LVCMOS25 2.5 V
LVCMOS18 1.8 V
```
#### 3.1.10. On-Chip Oscillator

The iCE40 UltraPlus devices feature two different frequency Oscillator. One is tailored for low-power operation that
runs at low frequency (LFOSC). Both Oscillators are controlled with internally generated current.

The LFOSC runs at nominal frequency of 10 kHz. The high frequency oscillator (HFOSC) runs at a nominal frequency of
48 MHz, divisible to 24 MHz, 12 MHz, or 6 MHz by user option. The LFOSC can be used to perform all always-on

functions, with the lowest power possible. The HFOSC can be enabled when the always-on functions detect a condition
that would need to wake up the system to perform higher frequency functions.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 3.1.11. User I^2 C IP

The iCE40 UltraPlus devices have two I^2 C IP cores. Either of the two cores can be configured either as an I^2 C master or

as an I^2 C slave. The pins for the I^2 C interface are not pre-assigned. User can use any General Purpose I/O pins.

In each of the two cores, there are options to delay the either the input or the output, or both, by 50 ns nominal, using
dedicated on-chip delay elements. This provides an easier interface with any external I^2 C components.

When the IP core is configured as master, it will be able to control other devices on the I^2 C bus through the pre-
assigned pin interface. When the core is configured as the slave, the device will be able to provide I/O expansion to an
I^2 C Master. The I^2 C cores support the following functionality:

 Master and Slave operation

 7 - bit and 10-bit addressing
 Multi-master arbitration support

 Clock stretching

 Up to 400 kHz data transfer speed

 General Call support
 Optionally delaying input or output data, or both

 Optional filter on SCL input

For further information on the User I^2 C, refer to iCE40 SPI/I2C Hardened IP Usage Guide (FPGA-TN- 02011 ).

#### 3.1.12. User SPI IP

The iCE40 UltraPlus devices have two SPI IP cores. The pins for the SPI interface are not pre-assigned. User can use any

General Purpose I/O pins. Both SPI IP cores can be configured as a SPI master or as a slave. When the SPI IP core is
configured as a master, it controls the other SPI enabled devices connected to the SPI Bus. When SPI IP core is
configured as a slave, the device will be able to interface to an external SPI master.

The SPI IP core supports the following functions:

 Configurable Master and Slave modes

 Full-Duplex data transfer
 Mode fault error flag with CPU interrupt capability

 Double-buffered data register

 Serial clock with programmable polarity and phase

 LSB First or MSB First Data Transfer

For further information on the User SPI, refer to iCE40 SPI/I2C Hardened IP Usage Guide (FPGA-TN- 02011 ).

#### 3.1.13. RGB High Current Drive I/O Pins

The iCE40 UltraPlus family devices offer multiple high current LED drive outputs in each device in the family to allow
the iCE40 UltraPlus product to drive LED signals directly on mobile applications.

There are three outputs on each device that can sink up to 24 mA current. These outputs are open-drain outputs, and
provides sinking current to an LED connecting to the positive supply. These three outputs are designed to drive the RBG
LEDs, such as the service LED found in a lot of mobile devices. This RGB drive current is user programmable from 4 mA

to 24 mA, in increments of 4 mA. This output functions as General Purpose I/O with open-drain when the high current
drive is not needed.

#### 3.1.14. RGB PWM IP

To provide an easier usage of the RGB high current drivers to drive RGB LED, a Pulse-Width Modulator IP can be used in
the user design. This PWM IP provides the flexibility for user to dynamically change the modulation width of each of
the RGB LED driver, which changes the color. Also, the user can dynamically change the settings on the ON-time
duration, OFF-time duration, and ability to turn the LED lights on and off gradually with user set breath-on and breath-

off time.

For additional information on the PWM IP, refer to iCE40 LED Driver Usage Guide (FPGA-TN- 02021 ).


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 3.1.15. Non-Volatile Configuration Memory

All iCE40 UltraPlus devices provide a Non-Volatile Configuration Memory (NVCM) block which can be used to configure
the device.

For more information on the NVCM, refer to iCE40 Programming and Configuration (FPGA-TN- 02001 ).

### 3.2. iCE40 UltraPlus Programming and Configuration

This section describes the programming and configuration of the iCE40 UltraPlus family.

#### 3.2.1. Device Programming

The NVCM memory can be programmed through the SPI port. The SPI port is located in Bank 1, using SPI_VCCIO1 power

supply.

#### 3.2.2. Device Configuration

There are various ways to configure the Configuration RAM (CRAM), using SPI port, including:

 From a SPI Flash (Master SPI mode)

 System microprocessor to drive a Serial Slave SPI port (SSPI mode)

For more details on configuring the iCE40 UltraPlus, refer to iCE40 Programming and Configuration (FPGA-TN- 02001 ).

#### 3.2.3. Power Saving Options

The iCE40 UltraPlus devices feature iCEGate and PLL low power mode to allow users to meet the static and dynamic

power requirements of their applications. Table 3. 11 describes the function of these features.

## Table 3.11. iCE40 UltraPlus Power Saving Features Description

```
Device Subsystem Feature Description
PLL When LATCHINPUTVALUE is enabled, puts the PLL into low-power mode; PLL output held static at last
input clock value.
iCEGate To save power, the optional iCEGate latch can selectively freeze the state of individual, non-registered
inputs within an I/O bank. Registered inputs are effectively frozen by their associated clock or clock-
enable control.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## 4. DC and Switching Characteristics

### 4.1. Absolute Maximum Ratings

## Table 4.1. Absolute Maximum Ratings

```
Parameter Min Max Unit
Supply Voltage VCC – 0.5 1.42 V
Output Supply Voltage VCCIO – 0.5 3.60 V
NVCM Supply Voltage VPP_2V5 – 0.5 3.60 V
PLL Supply Voltage VCCPLL – 0.5 1.42 V
I/O Tri-state Voltage Applied – 0.5 3.60 V
Dedicated Input Voltage Applied – 0.5 3.60 V
Storage Temperature (Ambient) – 65 150 °C
Junction Temperature (TJ) – 65 125 °C
```
**Notes** :

 Stress above those listed under the _Absolute Maximum Ratings_ may cause permanent damage to the device. Functional
operation of the device at these or any other conditions above those indicated in the operational sections of this specification is
not implied.
 Compliance with the Thermal Management document is required.

 All voltages referenced to GND.

### 4.2. Recommended Operating Conditions

## Table 4.2. Recommended Operating Conditions

```
Symbol Parameter Min Max Unit
VCC^1 Core Supply Voltage 1.14 1.26 V
```
##### VPP_2V5

##### VPP_2V5 NVCM

```
Programming and
Operating Supply Voltage
```
Slave SPI Configuration (^) 1.71^4 3.46 V
Master SPI Configuration (^) 2.30 3.46 V
Configuration from NVCM 2.30 3.46 V
NVCM Programming 2.30 3.00 V
VCCIO1, 2, 3 I/O Driver Supply Voltage VCCIO_0, SPI_VCCIO1, VCCIO_2 1.71 3.46 V
VCCPLL PLL Supply Voltage 1.14 1.26 V
tJCOM Junction Temperature Commercial Operation 0 85 °C
tJIND Junction Temperature, Industrial Operation – 40 100 °C
tPROG Junction Temperature NVCM Programming 10.00 30.00 °C
**Notes** :

1. Like power supplies must be tied together if they are at the same supply voltage and they meet the power up sequence
    requirement. See the Power-up Supply Sequence section. VCC and VCCPLL are recommended to be tied together to the same
    supply with an RC-based noise filter between them. Refer to iCE40 Hardware Checklist (FPGA-TN- 02006 ).
2. See recommended voltages by I/O standard in subsequent table.
3. VCCIO pins of unused I/O banks should be connected to the VCC power supply on boards.
4. VPP_2V5 can, optionally, be connected to a 1.8 V (+/–5%) power supply in Slave SPI Configuration modes subject to the condition
    that none of the HFOSC/LFOSC and RGB LED driver features are used. Otherwise, VPP_2V5 must be connected to a power supply
    with a minimum 2.30 V level.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.3. Power Supply Ramp Rates

## Table 4.3. Power Supply Ramp Rates

```
Symbol Parameter Min Max Unit
```
tRAMP Power supply ramp rates for all power supplies (^) 0.6 10 V/ms
**Notes** :
 Assumes monotonic ramp rates.
 Power up sequence must be followed. See the Power-up Supply Sequence section below.

### 4.4. Power-On Reset

All iCE40 UltraPlus devices have on-chip Power-On-Reset (POR) circuitry to ensure proper initialization of the device.

Only three supply rails are monitored by the POR circuitry as follows: (1) VCC, (2) SPI_VCCIO1 and (3) VPP_2V5. All other
supply pins have no effect on the power-on reset feature of the device. Note that all supply voltage pins must be
connected to power supplies for normal operation (including device configuration).

### 4.5. Power-up Supply Sequence...............................................................................................................................

It is recommended to bring up the power supplies in the following order. Note that there is no specified timing delay

between the power supplies, however, there is a requirement for each supply to reach a level of 0.5 V, or higher,
before any subsequent power supplies in the sequence are applied.

1. **VCC** and **VCCPLL** should be the first two supplies to be applied. Note that these two supplies can be tied together
    subject to the recommendation to include a RC-based noise filter on the VCCPLL. Refer to iCE40 Hardware Checklist
    (FPGA-TN- 02006 ).
2. **SPI_VCCIO1** should be the next supply, and can be applied any time after the previous supplies (VCC and VCCPLL) have
    reached as level of 0.5 V or higher.
3. **VPP_2V5** should be the next supply, and can be applied any time after previous supplies (VCC, VCCPLL and SPI_VCCIO1)
    have reached a level of 0.5 V or higher.
4. **Other Supplies** (VCCIO0 and VCCIO2) do not affect device power-up functionality, and they can be applied any time

```
after the initial power supplies (VCC and VCCPLL) have reached a level of 0.5 V or greater. There is no power down
sequence required. However, when partial power supplies are powered down, it is required the above sequence to
be followed when these supplies are re-powered up again.
```
### 4.6. External Reset

When all power supplies have reached their minimum operating voltage defined in Table 4. 2 , it is required to either

keep CRESET_B LOW, or toggle CRESET_B from HIGH to LOW, for a duration of tCRESET_B, and release it to go HIGH, to
start configuration download from either the internal NVCM or the external Flash memory. Figure 4. 1 shows Power-Up
sequence when SPI_VCCIO1 and VPP_2V5 are not connected together, and the CRESET_B signal triggers configuration
download. Figure 4. 2 shows when SPI_VCCIO1 and VPP_2V5 connected together. All power supplies should be powered
up during configuration. Before and during configuration, the I/Os are held in tri-state. I/Os are released to user

functionality once the device has finished configuration.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Figure 4.1. Power Up Sequence with SPE_VCCIO1 and VPP_2V5 Not Connected Together

## Figure 4.2. Power Up Sequence with All Supplies Connected Together to 1.8 V

### 4.7. Power-On-Reset Voltage Levels

## Table 4.4. Power-On-Reset Voltage Levels

```
Symbol Parameter Min Max Unit
```
##### VPORUP

```
Power-On-Reset ramp up trip point (circuit
monitoring VCC, SPI_VCCIO1, and^ VPP_2V5)
```
##### VCC 0.62 0.92 V

##### SPI_VCCIO1 0.87 1.50 V

##### VPP_2V5 0.90 1.53 V

##### VPORDN

```
Power-On-Reset ramp down trip point (circuit
monitoring VCC, SPI_VCCIO1, and^ VPP_2V5)
```
##### VCC — 0.79 V

##### SPI_VCCIO1 — 1.50 V

##### VPP_2V5 — 1.53 V

**Note** : These POR trip points are only provided for guidance. Device operation is only characterized for power supply voltages
specified under recommended operating conditions.

### 4.8. ESD Performance

Please contact Lattice Semiconductor for additional information.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.9. DC Electrical Characteristics

Over recommended operating conditions.

## Table 4.5. DC Electrical Characteristics

```
Symbol Parameter Condition Min Typ Max Unit
IIL, IIH1, 3,^4 Input or I/O Leakage^0 V < VIN < VCCIO + 0.2 V —^ —^ ±10^ μA^
```
```
C 1 I/O Capacitance, excluding
LED Drivers^2
```
##### VCCIO = 3.3 V, 2.5 V, 1.8 V

```
VCC = Typ, VIO = 0 to VCCIO + 0.2 V
```
```
— 6 — pf
```
```
C 2 Global Input Buffer^
Capacitance^2
```
##### VCCIO = 3.3 V, 2.5 V, 1.8 V

```
VCC = Typ, VIO = 0 to VCCIO + 0.2 V
```
```
— 6 — pf
```
```
C 3 RGB Pin Capacitance^2 VCC = Typ, VIO = 0 to 3.5 V — 15 — pf
C 4 IRLED Pin Capacitance^2 VCC = Typ, VIO = 0 to 3.5 V — 53 — pf
VHYST Input Hysteresis VCCIO = 1.8 V, 2.5 V, 3.3 V — 200 — mV
```
```
IPU Internal PIO Pull-up Current
```
```
VCCIO = 1. 8 V, 0 ≤ VIN ≤ 0.65 * VCCIO − 3 — − 31 μA
VCCIO = 2.5 V, 0 ≤ VIN ≤ 0.65 * VCCIO − 8 — − 72 μA
VCCIO = 3.3 V, 0 ≤ VIN ≤ 0.65 * VCCIO −^11 —^ −^128 μA^
```
**Notes:**

1. Input or I/O leakage current is measured with the pin configured as an input or as an I/O with the output driver tri-stated. It is
    not measured with the output driver active. Internal pull-up resistors are disabled.
2. TJ 25 oC, f = 1.0 MHz.
3. Refer to VIL and VIH in Table 4. 13.
4. Input pins are clamped to VCCIO and GND by a diode. When input is higher than VCCIO or lower than GND, the Input Leakage
    current will be higher than the IIL and IIH.

### 4.10. Supply Current

## Table 4.6. Supply Current

```
Symbol Parameter
```
```
Typ
VCC =1.2 V
```
```
Unit
```
```
ICCSTDBY Core Power Supply Static Current 75 μA
IPP2V5STDBY VPP_2V5 Power Supply Static Current 0.55 μA
ISPI_VCCIO1STDBY SPI_VCCIO1 Power Supply Static Current 0.5 μA
ICCIOSTDBY VCCIO Power Supply Static Current 0.5 μA
ICCPEAK Core Power Supply Startup Peak Current 12 mA
IPP_2V5PEAK VPP_2V5 Power Supply Startup Peak Current 2.5 mA
ISPI_VCCIO1PEAK SPI_VCCIO1 Power Supply Startup Peak Current 9.0 mA
ICCIOPEAK VCCIO Power Supply Startup Peak Current 2.0 mA
```
**Notes** :

 Assumes blank pattern with the following characteristics: all outputs are tri-stated, all inputs are configured as LVCMOS and
held at VCCIO or GND, on-chip PLL is off. For more detail with your specific design, use the Power Calculator tool. Power specified
with master SPI configuration mode. Other modes may be up to 25% higher.

 Frequency = 0 MHz.
 TJ = 25 °C, power supplies at nominal voltage, on devices processed in nominal process conditions.

 Does not include pull-up.
 Startup Peak Currents are measured with decoupling capacitances of 0.1 uF, 10 nF, and 1 nF to the power supply. Higher
decoupling capacitance causes higher current.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.11. User I^2 C Specifications

## Table 4.7. User I^2 C Specifications

```
Symbol Parameter
```
```
Spec (STD Mode) Spec (FAST Mode)
Unit
Min Typ Max Min Typ Max
fSCL Maximum SCL clock frequency — — 100 — — 400 kHz
tHI SCL clock HIGH Time 4 — — 0.6 — — μs
tLO SCL clock LOW Time 4.7 — — 1.3 — — μs
tSU,DAT Setup time (DATA) 250 — — 100 — — ns
tHD,DAT Hold time (DATA) 0 — — 0 — — ns
tSU,STA Setup time (START condition) 4.7 — — 0.6 — — μs
tHD,STA Hold time (START condition) 4 — — 0.6 — — μs
tSU,STO Setup time (STOP condition) 4 — — 0.6 — — μs
tBUF Bus free time between STOP and START 4.7 — — 1.3 — — μs
tCO,DAT SCL LOW to DATAOUT valid — — 3.4 — — 0.9 μs
```
### 4.12. I^2 C 50 ns Delay

## Table 4.8. I^2 C 50 ns Delay

```
Symbol Parameter
```
```
Spec
Unit
Min Typ Max
TDELAY Delay through 50 ns Delay Block — 50 — ns
```
### 4.13. I^2 C 50 ns Filter

## Table 4.9. I^2 C 50 ns Filter

```
Symbol Parameter
```
```
Spec
Unit
Min Typ Max
TFILTER-H HIGH Pulse Filter through 50 ns Filter Block — 50 — ns
TFILTER-L LOW Pulse Filter through 50 ns Filter Block — 50 — ns
```
### 4.14. User SPI Specifications

## Table 4.10. User SPI Specifications

```
Symbol Parameter Min Typ Max Unit
fMAX Maximum SCK clock frequency — — 45 MHz
```
**Notes** :

 All setup and hold time parameters on external SPI interface are design-specific and, therefore, generated by the Lattice Design
Software too. These parameters include the following:
 tSUmaster master Setup Time (master mode)
 tHOLDmaster master Hold time (master mode)
 tSUslave slave Setup Time (slave mode)
 tHOLDslave slave Hold time (slave mode)
 tSCK2OUT SCK to Out Delay (slave mode)
 The SCLK duty cycle needs to be specified in the Lattice Design Software as a timing constraint in order to ensure proper timing
check on SCLK HIGH and LOW (tHI, tLO) time.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.15. Internal Oscillators (HFOSC, LFOSC)

## Table 4.11. Internal Oscillators (HFOSC, LFOSC)

```
Parameter
Parameter Description
```
```
Spec/Recommended
Unit
Symbol Conditions Min Typ Max
```
```
fCLKHF
```
```
Commercial Temp HFOSC clock frequency (tJ = 0 °C– 85 °C) – 10% 48 10% MHz
Industrial Temp HFOSC clock frequency (tJ = – 40 °C– 100 °C) – 20% 48 20% MHz
fCLKLF — LFOSC CLKK clock frequency – 10% 10 10% kHz
```
```
DCHCLKHF
```
```
Commercial Temp HFOSC Duty Cycle (tJ = 0 °C– 85 °C) 45 50 55 %
Industrial Temp HFOSC Duty Cycle (tJ = – 40 °C– 100 °C) 40 50 60 %
DCHCLKLF — LFOSC Duty Cycle (Clock High Period) 45 50 55 %
Tsync_on — Oscillator output synchronizer delay — — 5 Cycles
Tsync_off — Oscillator output disable delay — — 5 Cycles
```
**Note** : Glitchless enabling and disabling OSC clock outputs.

### 4.16. sysI/O Recommended Operating Conditions

## Table 4.12. sysI/O Recommended Operating Conditions

```
Standard
```
##### VCCIO (V)

```
Min Typ Max
LVCMOS 3.3 3.14 3.3 3.46
LVCMOS 2.5 2.37 2.5 2.62
LVCMOS 1.8 1.71 1.8 1.89
```
### 4.17. sysI/O Single-Ended DC Electrical Characteristics

## Table 4.13. sysI/O Single-Ended DC Electrical Characteristics

```
Input/Output
Standard
```
```
VIL VIH VOL Max
(V)
```
```
VOH Min
(V)
```
##### IOL^

```
(mA)
```
```
IOH Max
Min (V) Max (V) Min (V) Max (V) (mA)^
```
```
LVCMOS 3. 3 – 0.3 0.8 2.0 VCCIO+0.2 V
```
##### 0.4 VCCIO − 0.4 8 – 8

##### 0.2 VCCIO − 0.2 0.1 – 0.1

##### LVCMOS 2. 5 – 0.3 0.7 1.7 VCCIO+0.2 V

##### 0.4 VCCIO − 0.4 6 – 6

##### 0.2 VCCIO − 0.2 0.1 – 0.1

##### LVCMOS 1. 8 – 0.3 0.35 VCCIO 0.65 VCCIO VCCIO+0.2 V

##### 0.4 VCCIO − 0.4 4 – 4

##### 0.2 VCCIO − 0.2 0.1 – 0.1


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.18. Differential Comparator Electrical Characteristics

## Table 4.14. Differential Comparator Electrical Characteristics

```
Parameter
Symbol
```
```
Parameter Description Test Conditions Min Max Unit
```
VREF (^) Reference Voltage to compare, on VINM VCCIO = 2.5 V 0.25 VCCIO - 0.25 V V
VDIFFIN_H (^) Differential input HIGH (VINP - VINM) VCCIO = 2.5 V 250 — mV
VDIFFIN_L Differential input LOW (VINP - VINM) VCCIO = 2.5 V — – 250 mV
IIN Input Current, VINP and VINM VCCIO = 2.5 V – 10 10 μA

### 4.19. Typical Building Block Function Performance

#### 4.19.1. Pin-to-Pin Performance (LVCMOS25)

## Table 4.15. Pin-to-Pin Performance (LVCMOS25)...............................................................................................................

```
Function Timing Unit
Basic Functions
16 - Bit Decoder 16.5 ns
4:1 Mux 18.0 ns
16:1 Mux 19.5 ns
```
**Notes** :
 The above timing numbers are generated using the Lattice Design Software tool. Exact performance may vary with device and
tool version. The tool uses internal parameters that have been characterized but are not tested on every device.

 Using a VCC of 1.14 V at Junction Temperature 85 °C.

#### 4.19.2. Register-to-Register Performance

## Table 4.16. Register-to-Register Performance....................................................................................................................

```
Function Timing Unit
Basic Functions
16:1 Mux 110 MHz
16 - Bit Adder 100 MHz
16 - Bit Counter 100 MHz
64 - Bit Counter 40 MHz
Embedded Memory Functions
256 x 16 Pseudo-Dual Port RAM 150 MHz
```
**Notes** :

 The above timing numbers are generated using the Lattice Design Software tool. Exact performance may vary with device and
tool version. The tool uses internal parameters that have been characterized but are not tested on every device.
 Under worst case operating conditions.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.20. sysDSP Timing

Over recommended operating conditions.

## Table 4.17. sysDSP Timing

```
Parameter Description Min Max Unit
fMAX8x8SMULT Max frequency signed MULT8x8 bypassing pipeline register — 50 MHz
fMAX16x16SMULT Max frequency signed MULT16x16 bypassing pipeline register — 50 MHz^
```
### 4.21. SPRAM Timing

Over recommended operating conditions.

## Table 4.18. Single Port RAM Timing

```
Parameter Description Min Max Unit
fMAXSRAM Max frequency SPRAM (4/8/16-bit Read and Write) 70 — MHz
```
### 4.22. Derating Logic Timing

Logic timing provided in the following sections of the data sheet and the Lattice design tools are worst case numbers in
the operating range. Actual delays may be much faster. Lattice design tools can provide logic timing numbers at a
particular temperature and voltage.

### 4.23. Maximum sysI/O Buffer Performance

## Table 4.19. Maximum sysI/O Buffer Performance

```
I/O Standard Max Speed Unit
Inputs
LVCMOS33 250 MHz
LVCMOS25 250 MHz
LVCMOS18 250 MHz
Outputs
LVCMOS33 250 MHz
LVCMOS25 250 MHz
LVCMOS18 155 MHz
LVCMOS1 2 70 MHz
```
**Note** : Measured with a toggling pattern.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.24. iCE40 UltraPlus Family Timing Adders

Over recommended commercial operating conditions.

## Table 4.20. iCE40 UltraPlus Family Timing Adders

```
Buffer Type Description Timing (Typ) Units
Input Adjusters
LVCMOS33 LVCMOS, VCCIO = 3.3 V 0.18 ns
LVCMOS25 LVCMOS, VCCIO = 2.5 V 0 ns
LVCMOS18 LVCMOS, VCCIO = 1.8 V 0.19 ns
Output Adjusters
LVCMOS33 LVCMOS, VCCIO = 3.3 V – 0.12 ns
LVCMOS25 LVCMOS, VCCIO = 2.5 V 0 ns
LVCMOS18 LVCMOS, VCCIO = 1.8 V 1.32 ns
LVCMOS1 2 LVCMOS, VCCIO = 1. 2 V 5 .3 8 ns
```
**Notes** :
 Timing adders are relative to LVCMOS25 and characterized but not tested on every device.

 LVCMOS timing measured with the load specified in the Switching Test Conditions table.
 Commercial timing numbers are shown.

### 4.25. iCE40 UltraPlus External Switching Characteristics

Over recommended commercial operating conditions.

## Table 4.21. iCE40 UltraPlus External Switching Characteristics

```
Parameter Description Device Min Max Unit
Clocks
Global Clock
fMAX_GBUF Frequency for Global Buffer Clock network All Devices — 185 MHz
tW_GBUF Clock Pulse Width for Global Buffer All Devices 2 — ns
tISKEW_GBUF Global Buffer Clock Skew Within a Device All Devices — 530 ps
Pin-LUT-Pin Propagation Delay
```
```
tPD Best case propagation delay through one
LUT logic
```
```
All Devices — 9.0 ns
```
```
General I/O Pin Parameters (Using Global Buffer Clock without PLL)*
tSKEW_IO Data bus skew across a bank of IOs All Devices — 510 ps
tCO Clock to Output – PIO Output Register All Devices — 10 .0 ns
tSU Clock to Data Setup – PIO Input Register All Devices −0.5 — ns
tH Clock to Data Hold – PIO Input Register All Devices 5.55 — ns
General I/O Pin Parameters (Using Global Buffer Clock with PLL)
tCOPLL Clock to Output – PIO Output Register All Devices — 2. 4 ns
tSUPLL Clock to Data Setup – PIO Input Register All Devices 7. 3 — ns
tHPLL Clock to Data Hold – PIO Input Register All Devices −1.1 — ns
```
***Note** : All the data is from the worst case.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.26. sysCLOCK PLL Timing

Over recommended operating conditions.

## Table 4.22. sysCLOCK PLL Timing

```
Parameter Descriptions Conditions Min Max Unit
```
```
fIN
```
```
Input Clock Frequency (REFERENCECLK,
EXTFEEDBACK) —^10 133 MHz^
fOUT Output Clock Frequency (PLLOUT) — 16 275 MHz
fVCO PLL VCO Frequency — 533 1066 MHz
fPFD^3 Phase Detector Input Frequency — 10 133 MHz
AC Characteristics
tDT Output Clock Duty Cycle — 40 60 %
tPH Output Phase Accuracy — — ±12 deg
```
```
tOPJIT1, 5, 6
```
```
Output Clock Period Jitter
```
```
fOUT >= 100 MHz — 450 ps p-p
fOUT < 100 MHz — 0.05 UIPP
```
```
Output Clock Cycle-to-Cycle Jitter
```
```
fOUT >= 100 MHz — 750 ps p-p
fOUT < 100 MHz — 0.10 UIPP
```
```
Output Clock Phase Jitter
```
```
fPFD >= 25 MHz — 275 ps p-p
fPFD < 25 MHz — 0.05 UIPP
tW Output Clock Pulse Width At 90% or 10% 1.33 — ns
tLOCK2, 3 PLL Lock-in Time — — 50 μs
tUNLOCK PLL Unlock Time — — 50 ns
```
```
tIPJIT^4 Input Clock Period Jitter
```
```
fPFD ≥ 20 MHz — 1000 ps p-p
fPFD < 20 MHz — 0.02 UIPP
tSTABLE^3 LATCHINPUTVALUE LOW to PLL Stable — — 500 ns
tSTABLE_PW^3 LATCHINPUTVALUE Pulse Width — 100 — ns
tRST RESET Pulse Width — 10 — ns
tRSTREC RESET Recovery Time — 10 — μs
tDYNAMIC_WD DYNAMICDELAY Pulse Width — 100 — VCO Cycles
```
**Notes:**

1. Period jitter sample is taken over 10,000 samples of the primary PLL output with a clean reference clock. Cycle-to-cycle jitter is
    taken over 1000 cycles. Phase jitter is taken over 2000 cycles. All values per JESD65B.
2. Output clock is valid after tLOCK for PLL reset and dynamic delay adjustment.
3. At minimum fPFD. As the fPFD increases the time will decrease to approximately 60% the value listed.
4. Maximum limit to prevent PLL unlock from occurring. Does not imply the PLL will operate within the output specifications listed
    in this table.
5. The jitter values will increase with loading of the PLD fabric and in the presence of SSO noise.

### 4.27. SPI Master or NVCM Configuration Time

## Table 4.23. SPI Master or NVCM Configuration Time

```
Symbol Parameter Conditions Max Unit
```
```
tCONFIG POR/CRESET_B to Device I/O Active
```
All devices – Low Frequency (Default) (^140) ms
All devices – Medium frequency 50 ms
All devices – High frequency^3
**Notes** :

1. Assumes sysMEM Block is initialized to an all zero pattern if they are used.
2. The NVCM download time is measured with a fast ramp rate starting from the maximum voltage of POR trip point.
3. High frequency is supported only on SPI Master.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.28. sysCONFIG Port Timing Specifications

Over recommended operating conditions.

## Table 4.24. sysCONFIG Port Timing Specifications

```
Symbol Parameter Conditions Min Typ Max Unit
All Configuration Mode
tCRESET_B Minimum CRESET_B LOW pulse width
required to restart configuration, from
falling edge to rising edge
```
```
— 200 — — ns
```
```
tDONE_IO Number of configuration clock cycles after
CDONE goes HIGH before the PIO pins are
activated
```
##### — 49 — —

```
Clock
Cycles
```
```
Slave SPI
tCR_SCK Minimum time from a rising edge on
CRESET_B until the first SPI WRITE
operation, first SPI_SCK clock. During this
time, the iCE40 UltraPlus device is clearing
its internal configuration memory
```
```
— 1200 — — μs
```
```
fMAX
CCLK clock frequency
```
```
Write 1 — 25 MHz
Read^1 — 15 — MHz
```
tCCLKH (^) CCLK clock pulsewidth HIGH — 20 — — ns
tCCLKL CCLK clock pulsewidth LOW — 20 — — ns
tSTSU CCLK setup time — 12 — — ns
tSTH CCLK hold time — 12 — — ns
tSTCO CCLK falling edge to valid output — 13 — — ns
**Master SPI**^3
fMCLK MCLK clock frequency Low Frequency
(Default)
7.0 12.0 17.0 MHz
Medium Frequency^2 21.0 33.0 45.0 MHz
High Frequency^2 33.0 53.0 71.0 MHz
tMCLK CRESET_B HIGH to first MCLK edge — 1200 — — μs
tSU CCLK setup time (^) — 6.16 — — ns
tHD CCLK hold time — 1 — — ns
**Notes** :

1. Supported with 1.2 V VCC and at 25 °C.
2. Extended range fMAX Write operations support up to 53 MHz with 1.2 V VCC and at 25 °C.
3. tSU and tHD timing must be met for all MCLK frequency choices.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 4.29. RGB LED Drive

## Table 4.25. RGB LED

```
Symbol Parameter Min Max Unit
ILED_ACCURACY RGB0, RGB1, RGB2 Sink Current Accuracy to selected current @ VLEDOUT >= 0.5 V – 12 +1 2 %
ILED_MATCH RGB0, RGB1, RGB2 Sink Current Matching among the 3 outputs @ VLEDOUT >= 0.5
V
```
##### – 5 +5 %

### 4.30. Switching Test Conditions

Figure 4. 3 shows the output test load that is used for AC testing. The specific values for resistance, capacitance,
voltage, and other test conditions are listed in Table 4. 26.

DUT

```
VT
```
R1

```
CL
```
```
Test Point
```
## Figure 4.3. Output Test Load, LVCMOS Standards

## Table 4.26. Test Fixture Required Components, Non-Terminated Interfaces

```
Test Condition R 1 CL Timing Reference VT
```
LVCMOS settings (L ≥ H, H ≥ L) (^) ∞ 0 pF

##### LVCMOS 3.3 = 1.5 V —

##### LVCMOS 2.5 = VCCIO/2 —

##### LVCMOS 1.8 = VCCIO/2 —

##### LVCMOS 3.3 (Z ≥ H)

```
188 0 pF
```
##### 1.5 V^ VOL

##### LVCMOS 3.3 (Z ≥ L) 1.5 V^ VOH

```
Other LVCMOS (Z ≥ H) VCCIO/2 VOL
Other LVCMOS (Z ≥ L) VCCIO/2 VOH
LVCMOS (H ≥ Z) VOH – 0.15 V VOL
LVCMOS (L ≥ Z) VOL – 0.15 V VOH
```
**Note** : Output test conditions for all other interfaces are determined by the respective standards.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## 5. Pinout Information

### 5.1. Signal Descriptions

#### 5.1.1. Power Supply Pins

```
Signal Name Function I/O Description
VCC Power — Core Power Supply
VCCIO_0, SPI_VCCIO1, VCCIO_2 Power — Power for I/Os in Bank 0, 1, and 2.
VPP_2V5 Power — Power for NVCM programming and operations.
VCCPLL Power — Power for PLL.
GND GROUND — Ground
GND_LED GROUND — Ground for LED drivers. Should connect to GND on board.
```
#### 5.1.2. Configuration Pins

```
Signal Name
General I/O Shared Function^ I/O^ Description^
Function
CRESET_B — Configuration I Configuration Reset, active LOW. No internal pull-up resistor.
Either actively driven externally or connect an 10 kΩ pull-up to
SPI_VCCIO1.
IOB_xxx CDONE Configuration I/O Configuration Done. Includes a weak pull-up resistor to
SPI_VCCIO1.
General I/O I/O In user mode, after configuration, this pin can be programmed
as general I/O in user function. In 30 - pin WLCSP, this pin
connects to IOB_12a, which also is shared as global signal G4
in user mode.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 5.1.3. Configuration SPI Pins

```
Signal Name
General I/O Shared Function^ I/O^ Description^
Function
IOB_34a SPI_SCK Configuration I/O This pin is shared with device configuration. During configuration:
In Master SPI mode, this pin outputs the clock to external SPI
memory.
In Slave SPI mode, this pin inputs the clock from external
processor.
General I/O I/O In user mode, after configuration, this pin can be programmed as
general I/O in user function.
IOB_32a SPI_SO Configuration Output This pin is shared with device configuration. During configuration:
In Master SPI mode, this pin outputs the command data to
external SPI memory.
In Slave SPI mode, this pin connects to the MISO pin of the
external processor.
General I/O I/O In user mode, after configuration, this pin can be programmed as
general I/O in user function.
IOB_33b SPI_SI Configuration Input This pin is shared with device configuration. During configuration:
In Master SPI mode, this pin receives data from external SPI
memory.
In Slave SPI mode, this pin connects to the MOSI pin of the
external processor.
General I/O I/O In user mode, after configuration, this pin can be programmed as
general I/O in user function.
IOB_35b SPI_SS Configuration I/O This pin is shared with device configuration. During configuration:
In Master SPI mode, this pin outputs to the external SPI memory.
In Slave SPI mode, this pin inputs CSN from the external
processor.
General I/O I/O In user mode, after configuration, this pin can be programmed as
general I/O in user function.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 5.1.4. Global Pins

```
Signal Name
General I/O Shared Function^ I/O^ Description^
Function
IOT_46b G0 General I/O I/O In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G0 pin drives the GBUF0 global buffer.
IOT_45a G1 General I/O I/O In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G1 pin drives the GBUF1 global buffer.
IOB_25b G3 General I/O I/O In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G3 pin drives the GBUF3 global buffer.
```
IOB_12a G4 General I/O I/O (^) In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G4 pin drives the GBUF4 global buffer.
IOB_11b G5 General I/O I/O In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G5 pin drives the GBUF5 global buffer.
IOB_3b G6 General I/O I/O In user mode, after configuration, this pin can be
programmed as general I/O in user function.
Global Input Global input used for high fanout, or clock/ reset net. The
G6 pin drives the GBUF6 global buffer.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
#### 5.1.5. General I/O, LED Pins

```
Signal Name
Function I/O Description
General I/O Shared Function
RGB0 — General I/O Open-
Drain I/O
```
```
In user mode, when RGB function is not used, this pin can
be connected to any user logic and used as open-drain I/O.
This pin is located in Bank 0.
LED Open-
Drain
Output
```
```
In user mode, when using RGB function, this pin can be
programmed as open drain 24 mA output to drive external
LED.
RGB1 — General I/O Open-
Drain I/O
```
```
In user mode, when RGB function is not used, this pin can
be connected to any user logic and used as open-drain I/O.
This pin is located in Bank 0.
LED Open-
Drain
Output
```
```
In user mode, when using RGB function, this pin can be
programmed as open drain 24 mA output to drive external
LED.
RGB2 — General I/O Open-
Drain I/O
```
```
In user mode, when RGB function is not used, this pin can
be connected to any user logic and used as open-drain I/O.
This pin is located in Bank 0.
LED Open-
Drain
Output
```
```
In user mode, when using RGB function, this pin can be
programmed as open drain 24 mA output to drive external
LED.
PIOT_xx — General I/O I/O In user mode, with user's choice, this pin can be
programmed as I/O in user function in the top (xx = I/O
location). These pins are located in Bank 0.
PIOB_xx — General I/O I/O In user mode, with user's choice, this pin can be
programmed as I/O in user function in the bottom (xx = I/O
location). Pins with xx <= 9 are located in Bank 2, pins with
xx> are located in Bank 1.
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 5.2. Pin Information Summary

```
Pin Type
```
```
iCE40UP3K iCE40UP5K
UWG30 UWG30 SG48
General Purpose I/O Per
Bank
```
```
Bank 0 7 7 17
Bank 1 10 10 14
Bank 2 4 4 8
Total General Purpose I/Os 21 21 39
VCC 1 1 2
VCCIO Bank 0 1 1 1
Bank 1 1 1 1
Bank 2 1 1 1
VCCPLL 1 1 1
VPP_2V5 1 1 1
Dedicated Config Pins 1 1 2
GND 2 2 0 *
Total Balls 30 30 48
```
***Note** : 48 - pin QFN package (SG48) requires the package paddle to be connected to GND.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
### 5.3. iCE40UP Part Number Description

```
Logic Cells
```
```
Packag e
```
```
Dev ice Family
```
#### i C E 40 U P X X - X X X X X I T R

```
3K = 2,800 Log ic Cells
5K = 5,280 Log ic Cells
```
```
All parts are shipped in tape -and-reel.
```
```
<blank> = Default Tape and Reel
for SG48 (See quantity below)
TR = Tape and Reel (See quantity below)
TR 50 = Tape and Reel, 50 units
TR1K = Tape and Reel, 1,000 units
```
```
Grade
I = Industrial
```
```
iCE 40 UP FPGA
```
##### TR

```
UWG30 = 30-Ball WLCSP (0.40 mm Ball Pitch)
SG 48 = 48-Pin QFN (0.50 mm Pin Pitch)
```
#### 5.3.1. Tape and Reel Quantity

```
Package TR Quantity
UWG30 5,000
SG48 2,000
```
### 5.4. Ordering Part Numbers

#### 5.4.1. Industrial

```
Part Number LUTs Supply Voltage Package Pins Temperature
iCE40UP 3 K-UWG30ITR 2800 1.2 V Halogen-Free WLCSP 30 IND
iCE40UP 3 K-UWG30ITR1K 2800 1.2 V Halogen-Free WLCSP 30 IND
iCE40UP3K-UWG30ITR 50 2800 1.2 V Halogen-Free WLCSP 30 IND
iCE40UP5K-SG48I 5280 1.2 V Halogen-Free QFN 48 IND
iCE40UP5K-SG48ITR50 5280 1.2 V Halogen-Free QFN 48 IND
iCE40UP 5 K-UWG30ITR 5280 1.2 V Halogen-Free WLCSP 30 IND
iCE40UP 5 K-UWG30ITR1K 5280 1.2 V Halogen-Free WLCSP 30 IND
iCE40UP 5 K-UWG30ITR 50 5280 1.2 V Halogen-Free WLCSP 30 IND
```

**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Supplemental Information

**For Further Information**

A variety of technical documents for the iCE40 UltraPlus family are available on the Lattice web site.
 iCE40 Programming and Configuration (FPGA-TN- 02001 )

 iCE40 SPI/I2C Hardened IP Usage Guide (FPGA-TN- 02010 )

 Advanced iCE40 SPI/I2C Hardened IP Usage Guide (FPGA-TN- 02011 )

 Memory Usage Guide for iCE40 Devices (FPGA-TN- 02002 )
 iCE40 sysCLOCK PLL Design and Usage Guide (FPGA-TN- 02052 )

 iCE40 Hardware Checklist (FPGA-TN- 02006 )

 iCE40 LED Driver Usage Guide (FPGA-TN- 02021 )

 DSP Function Usage Guide for iCE40 Devices (FPGA-TN- 02007 )
 iCE40 Oscillator Usage Guide (FPGA-TN- 02008 )

 iCE40 SPRAM Usage Guide (FPGA-TN- 02022 )

 iCE40 UltraPlus Pinout Files

 iCE40 UltraPlus Pin Migration Files
 Thermal Management

 Package Diagrams

 Lattice design tools


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Technical Support

For assistance, submit a technical support case at [http://www.latticesemi.com/techsupport.](http://www.latticesemi.com/techsupport.)


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
## Revision History

**Revision 1.9, December 2020**

```
Section Change Summary
```
DC and Switching Characteristics (^)  Updated values in Table 4. 17. sysDSP Timing.
 Updated footnotes in Table 4. 23. SPI Master or NVCM Configuration Time.
— Minor style adjustments
**Revision 1. 8 , August 2020
Section Change Summary**
Architecture  Removed paragraph regarding SCLK and SDI inputs from sysCLOCK Phase Locked Loops
(PLLs) section.
 Updated linked reference.
 Modified Figure 3.3. PLL Diagram.
Supplemental Information Updated document ID of sysCLOCK PLL Design and Usage Guide in For Further Information
section.
**Revision 1.7, February 2020
Section Change Summary**
Disclaimers Added this section.
**Revision 1.6, November 2018
Section Change Summary**
General Description Corrected product dimensions from 2.15 mm × 2.55 mm to 2.11 mm × 2.54 mm.
Product Family
**Revision 1.5, August 2018
Section Change Summary**
All Removed Copyright page.
DC and Switching Characteristics Updated sysCONFIG Port Timing Specifications section. Updated tCR_SCK parameter in Table
4.24.
Pinout Information Updated Configuration SPI Pins section.
Updated secondary signal name from SPI_SS_B to SPI_SS.
Supplemental Information Updated iCE40 Programming and Configuration document number.
**Revision 1.4, August 2017
Section Change Summary**
All  Changed document number from DS1056 to FPGA-DS-02008.
 Removed Preliminary from document cover page and header.


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
**Revision 1.3, August 2017**

```
Section Change Summary
```
All (^)  Changed document status from Advance to Preliminary.
 Updated footer.
Architecture (^)  Corrected link to iCE40 LED Driver Usage Guide (TN1288).
 Added link to iCE40 SPRAM Usage Guide (TN1314).
DC and Switching Characteristics  Updated Typ VCC=1.2 V values for IPP_2VSPEAK and ICOOPEAK in Table 4. 6. Supply Current.
 Added Min value for fMAXSRAM to Table 4. 18. Single Port RAM Timing.
 Added LVCMOS12 information to Table 4. 19. Maximum sysI/O Buffer Performance and
Table 4. 20. iCE40 UltraPlus Family Timing Adders.
 Updated Table 4. 21. iCE40 UltraPlus External Switching Characteristics. Revised Max
values for tISKEW_GBUF, tSKEW_IO, tCO, tCOPLL, and Min values for tSUPLL, tHPLL.
 Added Max values to Table 4.23. SPI Master or NVCM Configuration Time.
Pinout Information (^)  Updated TR description in the iCE40UP Part Number Description section.
 Updated part number information in the Ordering Part Numbers section.
Supplemental Information (^)  Corrected link to iCE40 LED Driver Usage Guide (TN1288).
 Added link to iCE40 SPRAM Usage Guide (TN1314).
 Added link to Package Diagrams.
**Revision 1.2, June 2016
Section Change Summary**
All Updated template.
Introduction Added QFN package in features list.
Product Family (^)  Added packages to Table 2.1. iCE40 UltraPlus Family Selection Guide.
 Added information on RGB PWM IP in Overview.
Architecture (^)  Performed minor editorial changes.
 Added information on 256 kb SPRAM blocks.
 Changed headings in Table 3.2. Global Buffer (GBUF) Connections to Programmable
Logic Blocks.
 Corrected VCCPLL format in Figure 3.3. PLL Diagram.
 Updated note in Table 3. 4. sysMEM Block Configurations.
 Added reference to iCE40 SPRAM Usage Guide (TN1314).
 Revised sysI/O Buffer Banks information.
 Corrected VCCIO format in Figure 3. 9. I/O Bank and Programmable I/O Cell.
 Revised Typical I/O Behavior During Power-up information.
 Revised Supported Standards information.
 Revised heading in Table 3.9. Supported Input Standards.
 Revised heading and removed LVCMOS12 in Table 3. 10 Table 3.10. Supported Output
Standards.
 Revised HFOSC information in On-Chip Oscillator section.
 Removed "An RGB PWM IP is also offered in the family." in RGB High Current Drive I/O
Pins section.
DC and Switching Characteristics (^)  Added the following figures:
 Figure 4.1. Power Up Sequence with SPE_VCCIO1 and VPP_2V5 Not Connected
Together.
 Figure 4.2. Power Up Sequence with All Supplies Connected Together to 1.8 V.
 Updated note in Table 4.5. DC Electrical Characteristics.
 Added note in Table 4.6. Supply Current.
 Revised User SPI Specifications 1, 2 section.
 Removed symbols.
 Added notes.
 Revised Table 4.11. Internal Oscillators (HFOSC, LFOSC).


**Data Sheet**

```
© 2018- 2020 Lattice Semiconductor Corp. All Lattice trademarks, registered trademarks, patents, and disclaimers are as listed at http://www.latticesemi.com/legal.
All other brand or product names are trademarks or registered trademarks of their respective holders. The specifications and information herein are subject to change without notice.
```
```
Section Change Summary
 Removed note in Table 4.13. sysI/O Single-Ended DC Electrical Characteristics.
 Changed to Lattice Design Software tool in Table 4.15. Pin-to-Pin Performance
(LVCMOS25).
 Changed to Lattice Design Software tool and revised note in Table 4.16. Register-to-
Register Performance.
 Added sysDSP Timing section.
 Added SPRAM Timing section.
 Removed LVCMOS12 and added timing values in Table 4.19. Maximum I/O Buffer
Performance.
 Removed LVCMOS12 and added timing values in Table 4.20. iCE40 UltraPlus Family
Timing Adders.
 Revised max values in Table 4.23. SPI Master or NVCM Configuration Time.
 Removed TBD conditions in Table 4.24. sysCONFIG Port Timing Specifications. Revised
tHD parameter.
 Revised Table 4.25. High Current RGB LED and IR LED Drive.
Pinout Information  General update to Signal Descriptions section.
 Updated the iCE40UP Part Number Description section. Added FGW49 package.
 Added OPNs.
Supplemental Information Added reference to FPGA-TN-02022, iCE40 SPRAM Usage Guide.
```
**Revision 1. 1 , September 2015
Section Change Summary**
Architecture Updated Architecture section. Replaced iCE5UP with iCE40UP.
Pinout Information Updated Pin Information section.
 Replaced iCE5UP with iCE40UP.
 Replaced SWG30 with UWG30.
Ordering Information Updated iCE40UP Part Number Description section.
 Replaced iCE5UP with iCE40UP.
 Replaced SWG30 with UWG30.
Updated Ordering Part Numbers section. Replaced the table of part
Further Information Removed reference to Schematic Symbols.

**Revision 1.0, August 2015**

```
Section Change Summary
All Initial release.
```

### http://www.latticesemi.com


