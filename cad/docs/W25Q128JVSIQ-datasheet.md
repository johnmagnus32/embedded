```
Publication Release Date: December 24, 2024
Revision M
```
3V 128 M-BIT

SERIAL FLASH MEMORY WITH

DUAL/QUAD SPI

For Industrial & Industrial Plus Grade


## - 1 -



- 1. GENERAL DESCRIPTIONS Table of Contents
- 2. FEATURES
- 3. PACKAGE TYPES AND PIN CONFIGURATIONS
   - 3.1 Pin Configuration SOIC 208-mil
   - 3.2 Pad Configuration WSON 6x5-mm/ 8x6-mm
   - 3.3 Pin Description SOIC 208-mil, WSON 6x5-mm / 8x6-mm
   - 3.4 Pin Configuration SOIC 300-mil
   - 3.5 Pin Description SOIC 300-mil
   - 3.6 Ball Configuration TFBGA 8x6-mm (5x5 or 6x4 Ball Array)
   - 3.7 Ball Description TFBGA 8x6-mm
   - 3.8 Ball Configuration WLCSP
   - 3.9 Ball Description WLCSP24
- 4. PIN DESCRIPTIONS
   - 4.1 Chip Select (/CS)
   - 4.2 Serial Data Input, Output and IOs (DI, DO and IO0, IO1, IO2, IO3)
   - 4.3 Write Protect (/WP)
   - 4.4 HOLD (/HOLD)
   - 4.5 Serial Clock (CLK)
   - 4.6 Reset (/RESET)
- 5. BLOCK DIAGRAM
- 6. FUNCTIONAL DESCRIPTIONS
   - 6.1 Standard SPI Instructions
   - 6.2 Dual SPI Instructions
   - 6.3 Quad SPI Instructions
   - 6.4 Software Reset & Hardware /RESET pin
   - 6.5 Write Protection
      - 6.5.1 Write Protect Features
- 7. STATUS AND CONFIGURATION REGISTERS
   - 7.1 Status Registers
      - 7.1.1 Erase/Write In Progress (BUSY) – Status Only
      - 7.1.2 Write Enable Latch (WEL) – Status Only
      - 7.1.3 Block Protect Bits (BP2, BP1, BP0) – Volatile/Non-Volatile Writable
      - 7.1.4 Top/Bottom Block Protect (TB) – Volatile/Non-Volatile Writable
      - 7.1.5 Sector/Block Protect Bit (SEC) – Volatile/Non-Volatile Writable
      - 7.1.6 Complement Protect (CMP) – Volatile/Non-Volatile Writable
      - 7.1.1 Status Register Protect (SRP, SRL) – Volatile/Non-Volatile Writable
         - Publication Release Date: December 24,
      - 7.1.2 Erase/Program Suspend Status (SUS) – Status Only - 2 - Revision M
      - 7.1.3 Security Register Lock Bits (LB3, LB2, LB1) – Non-Volatile OTP Writable
      - 7.1.4 Quad Enable (QE) – Volatile/Non-Volatile Writable
      - 7.1.5 Write Protect Selection (WPS) – Volatile/Non-Volatile Writable
      - 7.1.6 Output Driver Strength (DRV1, DRV0) – Volatile/Non-Volatile Writable
      - 7.1.7 Reserved Bits – Non Functional
      - 7.1.8 W25Q128JV Status Register Memory Protection (WPS = 0, CMP = 0)
      - 7.1.9 W25Q128JV Status Register Memory Protection (WPS = 0, CMP = 1)
      - 7.1.10 W25Q128JV Individual Block Memory Protection (WPS=1)
- 8. INSTRUCTIONS
   - 8.1 Device ID and Instruction Set Tables
      - 8.1.1 Manufacturer and Device Identification
      - 8.1.2 Instruction Set Table 1 (Standard SPI Instructions)(1)
      - 8.1.3 Instruction Set Table 2 (Dual/Quad SPI Instructions)
      - Notes:
   - 8.2 Instruction Descriptions
      - 8.2.1 Write Enable (06h)
      - 8.2.2 Write Enable for Volatile Status Register (50h)....................................................................
      - 8.2.3 Write Disable (04h)
      - 8.2.4 Read Status Register-1 (05h), Status Register- 2 ( 3 5h) & Status Register-3 (15h)
      - 8.2.5 Write Status Register-1 (01h), Status Register- 2 ( 3 1h) & Status Register-3 (11h)
      - 8.2.6 Read Data (03h)
      - 8.2.7 Fast Read (0Bh)
      - 8.2.8 Fast Read Dual Output (3Bh)
      - 8.2.9 Fast Read Quad Output (6Bh)
      - 8.2.10 Fast Read Dual I/O (BBh)
      - 8.2.11 Fast Read Quad I/O (EBh)
      - 8.2.12 Set Burst with Wrap (77h)
      - 8.2.13 Page Program (02h)
      - 8.2.14 Quad Input Page Program ( 3 2h)
      - 8.2.15 Sector Erase (20h)
      - 8.2.16 32KB Block Erase (52h)
      - 8.2.17 64KB Block Erase (D8h)
      - 8.2.18 Chip Erase (C7h / 60h)
      - 8.2.19 Erase / Program Suspend (75h)
      - 8.2.20 Erase / Program Resume (7Ah)
      - 8.2.21 Power-down (B9h)
      - 8.2.22 Release Power-down / Device ID (ABh)
      - 8.2.23 Read Manufacturer / Device ID (90h)
      - 8.2.24 Read Manufacturer / Device ID Dual I/O (92h)
      - 8.2.25 Read Manufacturer / Device ID Quad I/O (94h)
      - 8.2.26 Read Unique ID Number (4Bh) - 3 -
      - 8.2.27 Read JEDEC ID (9Fh)
      - 8.2.28 Read SFDP Register (5Ah)
      - 8.2.29 Erase Security Registers (44h)
      - 8.2.30 Program Security Registers (42h)
      - 8.2.31 Read Security Registers (48h)
      - 8.2.32 Individual Block/Sector Lock (36h)
      - 8.2.33 Individual Block/Sector Unlock (39h)..................................................................................
      - 8.2.34 Read Block/Sector Lock (3Dh)
      - 8.2.35 Global Block/Sector Lock (7Eh)
      - 8.2.36 Global Block/Sector Unlock (98h)
      - Enable Reset (66h) and Reset Device (99h)
- 9. ELECTRICAL CHARACTERISTICS
   - 9.1 Absolute Maximum Ratings (1)
   - 9.2 Operating Ranges...............................................................................................................
   - 9.3 Power-Up Power-Down Timing and Requirements
      - 9.3.1 Power Cycle Requirement
   - 9.4 DC Electrical Characteristics-
   - 9.5 AC Measurement Conditions
   - 9.6 AC Electrical Characteristics(6)
   - 9.7 Serial Output Timing
   - 9.8 Serial Input Timing
   - 9.9 /WP Timing
- 10. PACKAGE SPECIFICATIONS
   - 10.1 8 - Pin SOIC 208-mil (Package Code S)
   - 10.2 16 - Pin SOIC 300-mil (Package Code F)
   - 10.3 8 - Pad WSON 6x5-mm (Package Code P)
   - 10.4 8 - Pad WSON 8x6-mm (Package Code E)
   - 10.5 24 - Ball TFBGA 8x6-mm (Package Code B, 5x5-1 ball array)
   - 10.6 24 - Ball TFBGA 8x6-mm (Package Code C, 6x4 ball array)
   - 10.7 24 - Ball WLCSP (Package Code Y)
- 11. ORDERING INFORMATION
   - 11.1 Valid Part Numbers and Top Side Marking
- 12. REVISION HISTORY


```
Publication Release Date: December 24, 2024
```
- 4 - Revision M

## 1. GENERAL DESCRIPTIONS

The W25Q128JV (128M-bit) Serial Flash memory provides a storage solution for systems with limited
space, pins and power. The 25Q series offers flexibility and performance well beyond ordinary Serial Flash
devices. They are ideal for code shadowing to RAM, executing code directly from Dual/Quad SPI (XIP)
and storing voice, text and data. The device operates on a single 2.7V to 3.6V power supply with current
consumption as low as 1μA for power-down. All devices are offered in space-saving packages.

The W25Q128JV array is organized into 65,536 programmable pages of 256-bytes each. Up to 256 bytes
can be programmed at a time. Pages can be erased in groups of 16 (4KB sector erase), groups of 128
(32KB block erase), groups of 256 (64KB block erase) or the entire chip (chip erase). The W25Q128JV
has 4,096 erasable sectors and 256 erasable blocks respectively. The small 4KB sectors allow for greater
flexibility in applications that require data and parameter storage. (See Figure 2.)

The W25Q128JV supports the standard Serial Peripheral Interface (SPI), Dual/Quad I/O SPI: Serial
Clock, Chip Select, Serial Data I/O0 (DI), I/O1 (DO), I/O2 and I/O3. SPI clock frequencies of W25Q128JV
of up to 133MHz are supported allowing equivalent clock rates of 266 MHz (133MHz x 2) for Dual I/O and
532 MHz (133MHz x 4) for Quad I/O when using the Fast Read Dual/Quad I/O. These transfer rates can
outperform standard Asynchronous 8 and 16-bit Parallel Flash memories.

Additionally, the device supports JEDEC standard manufacturer and device ID and SFDP, and a 64-bit

Unique Serial Number and three 256-bytes Security Registers.

## 2. FEATURES

```
 New Family of SpiFlash Memories
```
- W25Q128JV: 128M-bit / 16 M-byte
- Standard SPI: CLK, /CS, DI, DO
- Dual SPI: CLK, /CS, IO 0 , IO 1
- Quad SPI: CLK, /CS, IO 0 , IO 1 , IO 2 , IO 3
- Software & Hardware Reset(1)
 Highest Performance Serial Flash
- 133 MHz Single, Dual/Quad SPI clocks
- 266 / 532 MHz equivalent Dual/Quad SPI
- 66 MB/S continuous data transfer rate
- Min. 100K Program-Erase cycles per sector
- More than 20-year data retention
 **Efficient “Continuous Read”**
- Continuous Read with 8/16/32/64-Byte Wrap
- As few as 8 clocks to address memory
- Allows true XIP (execute in place) operation
 Low Power, Wide Temperature Range
- Single 2.7 to 3.6V supply
- <1μA Power-down (typ.)
- - 40°C to +85°C operating range
- - 40°C to + 10 5°C operating range

```
 Flexible Architecture with 4KB sectors
```
- Uniform Sector/Block Erase (4K/32K/64K-Byte)
- Program 1 to 256 byte per programmable page
- Erase/Program Suspend & Resume
 Advanced Security Features
- Software and Hardware Write-Protect
- Power Supply Lock-Down
- Special OTP protection
- Top/Bottom, Complement array protection
- Individual Block/Sector array protection
- 64 - Bit Unique ID for each device
- Discoverable Parameters (SFDP) Register
- 3 X256-Bytes Security Registers with OTP locks
- Volatile & Non-volatile Status Register Bits^
 Space Efficient Packaging
- 8 - pin SOIC 208-mil
- 16 - pin SOIC 300-mil (additional /RESET pin)
- 8 - pad WSON 6x5-mm / 8x6-mm
- 24 - ball TFBGA 8x6-mm (6x4/5x5 ball array)
- 24 - ball WLCSP
- Contact Winbond for KGD and other options

```
Note: 1. Hardware /RESET pin is only available on
TFBGA or SOIC16 packages
```

##### - 5 -

## 3. PACKAGE TYPES AND PIN CONFIGURATIONS

### 3.1 Pin Configuration SOIC 208-mil

##### 1

##### 2

##### 3

##### 4

##### 8

##### 7

##### 6

##### 5

##### /CS

##### DO (IO 1 )

##### /WP (IO 2 )

##### GND

##### VCC

```
/HOLD or /RESET
(IO 3 )
```
##### DI (IO 0 )

##### CLK

```
Top View
```
```
Figure 1a. W25Q128JV Pin Assignments, 8-pin SOIC 208 - mil (Package Code S)
```
### 3.2 Pad Configuration WSON 6x5-mm/ 8x6-mm

##### 1

##### 2

##### 3

##### 4

##### /CS

##### DO (IO 1 )

##### /WP (IO 2 )

##### GND

##### VCC

```
/HOLD or /RESET
(IO 3 )
```
##### DI (IO 0 )

##### CLK

```
Top View
```
```
8
```
##### 7

##### 6

##### 5

```
Figure 1b. W25Q128JV Pad Assignments, 8-pad WSON 6x5-mm/ 8x6-mm (Package Code P/E)
```
### 3.3 Pin Description SOIC 208-mil, WSON 6x5-mm / 8x6-mm

```
PAD NO. PAD NAME I/O FUNCTION
1 /CS I Chip Select Input
2 DO (IO1) I/O Data Output (Data Input Output 1)(1)
3 /WP (IO2) I/O Write Protect Input ( Data Input Output 2)(2)
4 GND Ground
5 DI (IO0) I/O Data Input (Data Input Output 0)(1)
6 CLK I Serial Clock Input
```
```
7 /HOLD or /RESET(IO3) I/O Hold or Reset Input (Data Input Output 3)(2)
```
```
8 VCC Power Supply
```
```
Notes:
```
1. IO0 and IO1 are used for Standard and Dual SPI instructions
2. IO0 – IO3 are used for Quad SPI instructions, /HOLD (or /RESET) function is only available for Standard/Dual SPI.


```
Publication Release Date: December 24, 2024
```
- 6 - Revision M

### 3.4 Pin Configuration SOIC 300-mil

##### 1

##### 2

##### 3

##### 4

##### /CS

##### DO (IO 1 ) /WP (IO 2 )

##### GND

##### VCC

##### /HOLD (IO 3 )

##### DI (IO 0 )

##### CLK

```
Top View
```
##### NC

##### /RESET

##### NC

##### NC

##### NC

##### NC

##### NC

##### 5 NC

##### 6

##### 7

##### 8

##### 10

##### 9

##### 11

##### 12

##### 13

##### 14

##### 15

##### 16

```
Figure 1c. W25Q128JV Pin Assignments, 16-pin SOIC 300-mil (Package Code F)
```
### 3.5 Pin Description SOIC 300-mil

```
PIN NO. PIN NAME I/O FUNCTION
```
```
1
```
```
/HOLD or
/RESET (IO3)
```
```
I/O Hold or Reset Input (Data Input Output 3)(2)
```
```
2 VCC Power Supply
3 /RESET I Reset Input(3)
4 N/C No Connect
5 N/C No Connect
6 N/C No Connect
7 /CS I Chip Select Input
8 DO (IO1) I/O Data Output (Data Input Output 1)(1)
9 /WP (IO2) I/O Write Protect Input (Data Input Output 2)(2)
10 GND Ground
11 N/C No Connect
12 N/C No Connect
13 N/C No Connect
14 N/C No Connect
15 DI (IO0) I/O Data Input (Data Input Output 0)(1)
16 CLK I Serial Clock Input
```
```
Notes:
```
1. IO0 and IO1 are used for Standard and Dual SPI instructions.
2. IO0 – IO3 are used for Quad SPI instructions, /HOLD (or /RESET) function is only available for Standard/Dual SPI.
3. The /RESET pin is a dedicated hardware reset pin regardless of device settings or operation states. If the hardware reset
    function is not used, this pin can be left floating or connected to VCC in the system.


##### - 7 -

### 3.6 Ball Configuration TFBGA 8x6-mm (5x5 or 6x4 Ball Array)

```
D 1
DO(IO 1 ) DI(IO 0 )/HOLD(IO 3 )
```
```
/WP (IO 2 )
D 2 D 3 D 4
NC
E 1
NC NC NC
```
```
E 2 E 3 E 4
NC
F 1
NC NC NC
```
```
F 2 F 3 F 4
NC
```
```
A 1
NC NC /RESET
```
```
A 2 A 3 A 4
NC
B 1
CLK GND VCC
```
```
B 2 B 3 B 4
NC
C 1
/CS NC
```
```
C 2 C 3 C 4
NC
```
## Top View

```
Package Code C
```
## Top View

```
D 1
DO(IO 1 ) DI(IO 0 )/HOLD(IO 3 )
```
```
/WP (IO 2 )
D 2 D 3 D 4
NC
E 1
NC NC NC
```
```
E 2 E 3 E 4
NC
```
```
NC NC /RESET
```
```
A 2 A 3 A 4
```
```
B 1
CLK GND VCC
```
```
B 2 B 3 B 4
NC
C 1
/CS NC
```
```
C 2 C 3 C 4
NC
D 5
```
```
E 5
```
```
A 5
```
```
B 5
```
```
C 5
```
```
NC
```
```
NC
```
```
NC
```
```
NC
```
```
NC
```
```
Package Code B
```
```
Figure 1d. W25Q128JV Ball Assignments, 24-ball TFBGA 8x6-mm (Package Code B/C)
```
### 3.7 Ball Description TFBGA 8x6-mm

```
BALL NO. PIN NAME I/O FUNCTION
A4 /RESET I Reset Input(3)
B2 CLK I Serial Clock Input
B3 GND Ground
B4 VCC Power Supply
C2 /CS I Chip Select Input
C4 /WP (IO2) I/O Write Protect Input (Data Input Output 2)(2)
D2 DO (IO1) I/O Data Output (Data Input Output 1)(1)
D3 DI (IO0) I/O Data Input (Data Input Output 0)(1)
D4 /HOLD (IO3) I/O Hold or Reset Input (Data Input Output 3)(2)
Multiple NC No Connect
```
```
Notes:
```
1. IO0 and IO1 are used for Standard and Dual SPI instructions
2. IO0 – IO3 are used for Quad SPI instructions, /HOLD (or /RESET) function is only available for Standard/Dual SPI.
3. The /RESET pin is a dedicated hardware reset pin regardless of device settings or operation states.
If the hardware reset function is not used, this pin can be left floating or connected to VCC in the system


```
Publication Release Date: December 24, 2024
```
- 8 - Revision M

### 3.8 Ball Configuration WLCSP

## Bottom View

```
A 1 A 4
NC
```
## Top View

```
A 3
NC
```
```
A 2
NC NC
```
```
B 1 B 4
NC
```
```
B 2 B 3
NC
```
```
C 1 C 4
NC
```
```
C 2 C 3
NC
```
```
D 1 D 4
NC
```
```
D 2 D 3
NC
```
```
E 1 E 4
NC
```
```
E 2 E 3
NC
```
```
F 1 F 4
NC
```
```
F 3
NC
```
```
F 2
NC NC
```
```
A 4 A 1
NC
```
```
A 2
NC
```
```
A 3
NC NC
```
```
B 4 B 1
NC
```
```
B 3 B 2
NC
```
```
C 4 C 1
NC
```
```
C 3 C 2
NC
```
```
D 4 D 1
NC
```
```
D 3 D 2
NC
```
```
E 4 E 1
NC
```
```
E 3 E 2
NC
```
```
F 3 F 1
NC
```
```
F 2
NC
```
```
F 3
NC NC
```
```
VCC /CS
```
```
/HOLD(IO 3 ) DO(IO 1 )
```
```
CLK /WP(IO 2 )
```
```
DI(IO 0 ) GND
```
```
/CS
```
```
DO(IO 1 )
```
```
/WP(IO 2 )
```
```
GND
```
```
VCC
```
```
/HOLD (IO 3 )
```
```
CLK
```
```
DI(IO 0 )
```
```
Figure 1e. W25Q128JV Ball Assignments, 24 - ball WLCSP (Package Code Y)
```
### 3.9 Ball Description WLCSP24

```
BALL NO. PIN NAME I/O FUNCTION
```
```
B2 VCC Power Supply
B3 /CS I Chip Select Input
C2 /HOLD (IO3) I/O Hold Input (Data Input Output 3)*^2
C3 DO (IO1) I/O Data Output (Data Input Output 1)*^1
D2 CLK I Serial Clock Input
D3 /WP (IO2) I/O Write Protect Input (Data Input Output 2)*^2
E2 DI (IO0) I/O Data Input (Data Input Output 0)*^1
E3 GND Ground
```
```
Notes:
1. IO0 and IO1 are used for Standard and Dual SPI instructions
2. IO0 – IO3 are used for Quad SPI instructions, /HOLD (or /RESET) function is only available for Standard/Dual SPI.
```

##### - 9 -

## 4. PIN DESCRIPTIONS

### 4.1 Chip Select (/CS)

The SPI Chip Select (/CS) pin enables and disables device operation. When /CS is high the device is
deselected and the Serial Data Output (DO, or IO0, IO1, IO2, IO3) pins are at high impedance. When
deselected, the devices power consumption will be at standby levels unless an internal erase, program or
write status register cycle is in progress. When /CS is brought low the device will be selected, power
consumption will increase to active levels and instructions can be written to and data read from the device.
After power-up, /CS must transition from high to low before a new instruction will be accepted. The /CS
input must track the VCC supply level at power-up and power-down (see “Write Protection” and Figure
5 8). If needed a pull-up resister on the /CS pin can be used to accomplish this.

### 4.2 Serial Data Input, Output and IOs (DI, DO and IO0, IO1, IO2, IO3)

The W25Q128JV supports standard SPI, Dual SPI and Quad SPI operation. Standard SPI instructions
use the unidirectional DI (input) pin to serially write instructions, addresses or data to the device on the
rising edge of the Serial Clock (CLK) input pin. Standard SPI also uses the unidirectional DO (output) to
read data or status from the device on the falling edge of CLK.

Dual and Quad SPI instructions use the bidirectional IO pins to serially write instructions, addresses or
data to the device on the rising edge of CLK and read data or status from the device on the falling edge of
CLK. Quad SPI instructions require the non-volatile Quad Enable bit (QE) in Status Register-2 to be set.
When QE=1, the /WP pin becomes IO2 and the /HOLD pin becomes IO3.

### 4.3 Write Protect (/WP)

The Write Protect (/WP) pin can be used to prevent the Status Register from being written. Used in
conjunction with the Status Register’s Block Protect (CMP, SEC, TB, BP2, BP1 and BP0) bits and Status
Register Protect (SRP) bits, a portion as small as a 4KB sector or the entire memory array can be
hardware protected. The /WP pin is active low.

### 4.4 HOLD (/HOLD)

The /HOLD pin allows the device to be paused while it is actively selected. When /HOLD is brought low,
while /CS is low, the DO pin will be at high impedance and signals on the DI and CLK pins will be ignored
(don’t care). When /HOLD is brought high, device operation can resume. The /HOLD function can be
useful when multiple devices are sharing the same SPI signals. The /HOLD pin is active low. When the
QE bit of Status Register-2 is set for Quad I/O, the /HOLD pin function is not available since this pin is
used for IO3. See Figure 1a-c for the pin configuration of Quad I/O operation.

### 4.5 Serial Clock (CLK)

The SPI Serial Clock Input (CLK) pin provides the timing for serial input and output operations. ("See SPI
Operations")

### 4.6 Reset (/RESET)

A dedicated hardware /RESET pin is available on SOIC-16 and TFBGA packages. When it’s driven low for
a minimum period of ~1μS, this device will terminate any external or internal operations and return to its
power-on state.

Note: Hardware /RESET pin is available on SOIC- 16 or TFBGA; please contact Winbond for this package.


```
Publication Release Date: December 24, 2024
```
- 10 - Revision M

## 5. BLOCK DIAGRAM

```
Figure 2. W 25 Q128JV Serial Flash Memory Block Diagram
```
```
003000h 0030FFh
002000h 0020FFh
001000h 0010FFh
```
```
Column Decode
And 256-Byte Page Buffer
```
```
Beginning
Page Address
```
```
Ending
Page Address
```
```
W25Q128FV
```
```
SPI
Command &
Control Logic
```
```
Byte Address
Latch / Counter
```
```
Status
Register
```
```
Write Control
Logic
```
```
Page Address
Latch / Counter
```
```
DO (IO 1 )
```
```
DI (IO 0 )
```
```
/CS
```
```
CLK
```
```
/HOLD (IO 3 ) or
/RESET (IO 3 )
```
```
/WP (IO 2 )
```
```
High Voltage
Generators
```
```
xx0F00h xx0FFFh
```
- Sector 0 (4KB) •
xx0000h xx00FFh

```
xx1F00h xx1FFFh
```
- Sector 1 (4KB) •
xx1000h xx10FFh

```
xx2F00h xx2FFFh
```
- Sector 2 (4KB) •
xx2000h xx20FFh
-
-
-

```
xxDF00h xxDFFFh
```
- Sector 13 (4KB) •
xxD000h xxD0FFh

```
xxEF00h xxEFFFh
```
- Sector 14 (4KB) •
xxE000h xxE0FFh

```
xxFF00h xxFFFFh
```
- Sector 15 (4KB) •
xxF000h xxF0FFh

```
Block Segmentation
```
```
Data
```
```
Security Register 1 - 3
```
```
Write Protect Logic and Row Decode
```
```
000000h 0000FFh
```
```
SFDP Register
```
```
00FF00h 00FFFFh
```
- Block 0 (64KB) •
000000h 0000FFh
-
-
-

```
3FFF00h 3FFFFFh
```
- Block 63 (64KB) •
3F0000h 3F00FFh

```
40FF00h 40FFFFh
```
- Block 64 (64KB) •
400000h 4000FFh
-
-
-

```
7FFF00h 7FFFFFh
```
- Block 127 (64KB) •
7F0000h 7F00FFh

```
80FF00h 80FFFFh
```
- Block 128 (64KB) •
800000h 8000FFh
-
-
-

```
FFFF00h FFFFFFh
```
- Block 255 (64KB) •
FF0000h FF00FFh

```
W
```
```
25
```
```
Q
```
```
12
```
```
8 J
```
```
V
```

##### - 11 -

## 6. FUNCTIONAL DESCRIPTIONS

### 6.1 Standard SPI Instructions

The W25Q128JV is accessed through an SPI compatible bus consisting of four signals: Serial Clock
(CLK), Chip Select (/CS), Serial Data Input (DI) and Serial Data Output (DO). Standard SPI instructions
use the DI input pin to serially write instructions, addresses or data to the device on the rising edge of
CLK. The DO output pin is used to read data or status from the device on the falling edge of CLK.

SPI bus operation Mode 0 (0,0) and 3 (1,1) are supported. The primary difference between Mode 0 and
Mode 3 concerns the normal state of the CLK signal when the SPI bus master is in standby and data is
not being transferred to the Serial Flash. For Mode 0, the CLK signal is normally low on the falling and
rising edges of /CS. For Mode 3, the CLK signal is normally high on the falling and rising edges of /CS.

### 6.2 Dual SPI Instructions

The W25Q128JV supports Dual SPI operation when using instructions such as “Fast Read Dual Output
(3Bh)” and “Fast Read Dual I/O (BBh)”. These instructions allow data to be transferred to or from the
device at two to three times the rate of ordinary Serial Flash devices. The Dual SPI Read instructions are
ideal for quickly downloading code to RAM upon power-up (code-shadowing) or for executing non-speed-
critical code directly from the SPI bus (XIP). When using Dual SPI instructions, the DI and DO pins
become bidirectional I/O pins: IO0 and IO1.

### 6.3 Quad SPI Instructions

The W25Q128JV supports Quad SPI operation when using instructions such as “Fast Read Quad Output
(6Bh)”, and “Fast Read Quad I/O (EBh). These instructions allow data to be transferred to or from the
device four to six times the rate of ordinary Serial Flash. When using Quad SPI instructions, the DI and
DO pins become bidirectional IO0 and IO1, with the additional I/O pins: IO2, IO3.

### 6.4 Software Reset & Hardware /RESET pin

The W25Q128JV can be reset to the initial power-on state by a software Reset sequence. This sequence
must include two consecutive instructions: Enable Reset (66h) & Reset (99h). If the instruction sequence
is successfully accepted, the device will take approximately 30μS (tRST) to reset. No instruction will be
accepted during the reset period. For the SOIC- 16 and TFBGA packages, W25Q128JV provides a
dedicated hardware /RESET pin. Drive the /RESET pin low for a minimum period of ~1μS (tRESET*) will
interrupt any on-going external/internal operations and reset the device to its initial power-on state.
Hardware /RESET pin has higher priority than other SPI input signals (/CS, CLK, IOs).

Note:

1. Hardware /RESET pin is available on SOIC- 16 or TFBGA; please contact Winbond for his package.
2. While a faster /RESET pulse (as short as a few hundred nanoseconds) will often reset the device, a 1us minimum is
    recommended to ensure reliable operation.
3. There is an internal pull-up resistor for the dedicated /RESET pin on the SOIC- 16 and TFBGA- 24 package. If the reset function
    is not needed, this pin can be left floating in the system.


```
Publication Release Date: December 24, 2024
```
- 12 - Revision M

### 6.5 Write Protection

Applications that use non-volatile memory must take into consideration the possibility of noise and other
adverse system conditions that may compromise data integrity. To address this concern, the W25Q128JV
provides several means to protect the data from inadvertent writes.

#### 6.5.1 Write Protect Features

 Device resets when VCC is below threshold
 Time delay write disable after Power-up

 Write enable/disable instructions and automatic write disable after erase or program
 Software and Hardware (/WP pin) write protection using Status Registers
 Additional Individual Block/Sector Locks for array protection
 Write Protection using Power-down instruction
 Lock Down write protection for Status Register until the next power-up
 One Time Program (OTP) write protection for array and Security Registers using Status Register*
* Note: This feature is available upon special flow. Please contact Winbond for details.

Upon power-up or at power-down, the W25Q128JV will maintain a reset condition while VCC is below the
threshold value of VWI, (See Power-up Timing and Voltage Levels and Figure 43). While reset, all
operations are disabled and no instructions are recognized. During power-up and after the VCC voltage
exceeds VWI, all program and erase related instructions are further disabled for a time delay of tPUW. This
includes the Write Enable, Page Program, Sector Erase, Block Erase, Chip Erase and the Write Status
Register instructions. Note that the chip select pin (/CS) must track the VCC supply level at power-up until
the VCC-min level and tVSL time delay is reached, and it must also track the VCC supply level at power-
down to prevent adverse command sequence. If needed a pull-up resister on /CS can be used to
accomplish this.

After power-up the device is automatically placed in a write-disabled state with the Status Register Write
Enable Latch (WEL) set to a 0. A Write Enable instruction must be issued before a Page Program, Sector
Erase, Block Erase, Chip Erase or Write Status Register instruction will be accepted. After completing a
program, erase or write instruction the Write Enable Latch (WEL) is automatically cleared to a write-
disabled state of 0.

Software controlled write protection is facilitated using the Write Status Register instruction and setting the
Status Register Protect (SRP, SRL) and Block Protect (CMP, TB, BP[3:0]) bits. These settings allow a
portion or the entire memory array to be configured as read only. Used in conjunction with the Write
Protect (/WP) pin, changes to the Status Register can be enabled or disabled under hardware control. See
Status Register section for further information. Additionally, the Power-down instruction offers an extra
level of write protection as all instructions are ignored except for the Release Power-down instruction.

The W25Q128JV also provides another Write Protect method using the Individual Block Locks. Each
64KB block (except the top and bottom blocks, total of 126 blocks) and each 4KB sector within the
top/bottom blocks (total of 32 sectors) are equipped with an Individual Block Lock bit. When the lock bit is
0, the corresponding sector or block can be erased or programmed; when the lock bit is set to 1, Erase or
Program commands issued to the corresponding sector or block will be ignored. When the device is
powered on, all Individual Block Lock bits will be 1, so the entire memory array is protected from
Erase/Program. An “Individual Block Unlock (39h)” instruction must be issued to unlock any specific sector
or block.

The WPS bit in Status Register-3 is used to decide which Write Protect scheme should be used. When
WPS=0 (factory default), the device will only utilize CMP, SEC, TB, BP[2:0] bits to protect specific areas of
the array; when WPS=1, the device will utilize the Individual Block Locks for write protection.


##### - 13 -

## 7. STATUS AND CONFIGURATION REGISTERS

Three Status and Configuration Registers are provided for W25Q128JV. The Read Status Register-1/2/
instructions can be used to provide status on the availability of the flash memory array, whether the device
is write enabled or disabled, the state of write protection, Quad SPI setting, Security Register lock status,
Erase/Program Suspend status, output driver strength, power-up. The Write Status Register instruction
can be used to configure the device write protection features, Quad SPI setting, Security Register OTP locks,
and output driver strength. Write access to the Status Register is controlled by the state of the non-volatile
Status Register Protect bits (SRL), the Write Enable instruction, and during Standard/Dual SPI operations

### 7.1 Status Registers

## (volatile/non-volatile)

## (volatile/non-volatile)

## (volatile/non-volatile)

```
Figure 4a. Status Register- 1
```
#### 7.1.1 Erase/Write In Progress (BUSY) – Status Only

BUSY is a read only bit in the status register (S0) that is set to a 1 state when the device is executing a
Page Program, Quad Page Program, Sector Erase, Block Erase, Chip Erase, Write Status Register or
Erase/Program Security Register instruction. During this time the device will ignore further instructions
except for the Read Status Register and Erase/Program Suspend instruction (see tW, tPP, tSE, tBE, and
tCE in AC Characteristics). When the program, erase or write status/security register instruction has
completed, the BUSY bit will be cleared to a 0 state indicating the device is ready for further instructions.

#### 7.1.2 Write Enable Latch (WEL) – Status Only

Write Enable Latch (WEL) is a read only bit in the status register (S1) that is set to 1 after executing a
Write Enable Instruction. The WEL status bit is cleared to 0 when the device is write disabled. A write
disable state occurs upon power-up or after any of the following instructions: Write Disable, Page
Program, Quad Page Program, Sector Erase, Block Erase, Chip Erase, Write Status Register, Erase
Security Register and Program Security Register.

#### 7.1.3 Block Protect Bits (BP2, BP1, BP0) – Volatile/Non-Volatile Writable

The Block Protect Bits (BP2, BP1, BP0) are non-volatile read/write bits in the status register (S4, S3, and
S2) that provide Write Protection control and status. Block Protect bits can be set using the Write Status
Register Instruction (see tW in AC characteristics). All, none or a portion of the memory array can be
protected from Program and Erase instructions (see Status Register Memory Protection table). The
factory default setting for the Block Protection Bits is 0, none of the array protected.


```
Publication Release Date: December 24, 2024
```
- 14 - Revision M

#### 7.1.4 Top/Bottom Block Protect (TB) – Volatile/Non-Volatile Writable

The non-volatile Top/Bottom bit (TB) controls if the Block Protect Bits (BP2, BP1, BP0) protect from the
Top (TB=0) or the Bottom (TB=1) of the array as shown in the Status Register Memory Protection table.
The factory default setting is TB=0. The TB bit can be set with the Write Status Register Instruction
depending on the state of the SRP, SRL and WEL bits.

#### 7.1.5 Sector/Block Protect Bit (SEC) – Volatile/Non-Volatile Writable

The non-volatile Sector/Block Protect bit (SEC) controls if the Block Protect Bits (BP2, BP1, BP0) protect
either 4KB Sectors (SEC=1) or 64KB Blocks (SEC=0) in the Top (TB=0) or the Bottom (TB=1) of the array
as shown in the Status Register Memory Protection table. The default setting is SEC=0.

#### 7.1.6 Complement Protect (CMP) – Volatile/Non-Volatile Writable

The Complement Protect bit (CMP) is a non-volatile read/write bit in the status register (S14). It is used in
conjunction with SEC, TB, BP2, BP1 and BP0 bits to provide more flexibility for the array protection. Once
CMP is set to 1, previous array protection set by SEC, TB, BP2, BP1 and BP0 will be reversed. For
instance, when CMP=0, a top 6 4KB block can be protected while the rest of the array is not; when
CMP=1, the top 6 4KB block will become unprotected while the rest of the array become read-only. Please
refer to the Status Register Memory Protection table for details. The default setting is CMP=0.


##### - 15 -

#### 7.1.1 Status Register Protect (SRP, SRL) – Volatile/Non-Volatile Writable

Three Status and Configuration Registers are provided for W25Q128JV. The Read Status Register-1/2/
instructions can be used to provide status on the availability of the flash memory array, whether the device
is write enabled or disabled, the state of write protection, Quad SPI setting, Security Register lock status,
Erase/Program Suspend status,and output driver strength, The Write Status Register instruction can be
used to configure the device write protection features, Quad SPI setting, Security Register OTP locks,
output driver. Write access to the Status Register is controlled by the state of the non-volatile Status
Register Protect bits (SRP, SRL), the Write Enable instruction, and during Standard/Dual SPI operations,
the /WP pin.

##### SRL SRP /WP

```
Status
Register Description^
```
##### 0 0 X

```
Software
Protection
```
```
/WP pin has no control. The Status register can be written to
after a Write Enable instruction, WEL=1. [Factory Default]
```
##### 0 1 0

```
Hardware
Protected
```
```
When /WP pin is low the Status Register locked and cannot be
written to.
```
0 1 1 Hardware^
Unprotected

```
When /WP pin is high the Status register is unlocked and can
be written to after a Write Enable instruction, WEL=1.
```
##### 1 X X

```
Power Supply
Lock-Down
```
```
Status Register is protected and cannot be written to again until
the next power-down, power-up cycle.(1)
```
##### 1 X X

```
One Time
Program(2)
```
```
Status Register is permanently protected and cannot be written
to. (enabled by adding prefix command AAh, 55h)
```
1. When SRL = 1 , a power-down, power-up cycle will change SRL = 0 state.
2. Please contact Winbond for details regarding the special instruction sequence.


```
Publication Release Date: December 24, 2024
```
- 16 - Revision M

```
S 15 S 14 S 13 S 12 S 11 S 10 S 9 S 8
```
```
SUS CMP LB 3 LB 2 LB 1 (R) QE
```
```
SUSPEND STATUS
(Status Only)
COMPLEMENT PROTECT
(Volatile/Non-Volatile Writable)
SECURITY REGISTER LOCK BITS
(Non-Volatile OTP Writable)
```
```
S 15 S 14 S 13 S 12 S 11 S 10 S 9
```
```
SUS CMP LB 3 LB 2 LB 1 SRL
```
```
QUAD ENABLE
(Volatile/Non-Volatile OTP Writable)
STATUS REGISTER LOCK
(Volatile/Non-Volatile Writable)
```
```
Reserved
```
```
Figure 4b. Status Register- 2
```
#### 7.1.2 Erase/Program Suspend Status (SUS) – Status Only

The Suspend Status bit is a read only bit in the status register (S15) that is set to 1 after executing a
Erase/Program Suspend (75h) instruction. The SUS status bit is cleared to 0 by Erase/Program Resume
(7Ah) instruction as well as a power-down, power-up cycle.

#### 7.1.3 Security Register Lock Bits (LB3, LB2, LB1) – Non-Volatile OTP Writable

The Security Register Lock Bits (LB3, LB2, LB1) are non-volatile One Time Program (OTP) bits in Status
Register (S13, S12, S11) that provide the write protect control and status to the Security Registers. The
default state of LB3- 1 is 0, Security Registers are unlocked. LB3- 1 can be set to 1 individually using the
Write Status Register instruction. LB3- 1 are One Time Programmable (OTP), once it’s set to 1, the
corresponding 256-Byte Security Register will become read-only permanently.

#### 7.1.4 Quad Enable (QE) – Volatile/Non-Volatile Writable

The Quad Enable (QE) bit is a non-volatile read/write bit in the status register (S 9 ) that enables Quad SPI
operation. When the QE bit is set to a 0 state (factory default for part numbers with ordering options “IM” &
“JM”), the /HOLD is enabled, the device operates in Standard/Dual SPI modes. When the QE bit is set to
a 1 (factory fixed default for part numbers with ordering options “IQ/IN” & “JQ”), the Quad IO2 and IO
pins are enabled, and /HOLD function is disabled, the device operates in Standard/Dual/Quad SPI modes.

Note: QE bit is set to a 0 state, factory default for part numbers with ordering options “IM” or ”JM”; please
see W25Q 128 JV-M DTR data sheet.


##### - 17 -

```
R
```
```
S 23 S 22 S 21 S 20 S 19 S 18 S 17 S 16
```
```
HOLD
/RST
```
```
DRV 1 DRV 0 WPS
```
```
Output Driver Strength
```
```
Write Protect Selection
```
```
/HOLD or /RESET Function
```
```
R
```
```
Reserved
```
```
Reserved
```
```
(Volatile/Non-Volatile Writable)
```
```
(Volatile/Non-Volatile Writable)
```
```
(Volatile/Non-Volatile Writable)
```
```
(R) (R)
```
```
Reserved
```
```
Figure 4c. Status Register- 3
```
#### 7.1.5 Write Protect Selection (WPS) – Volatile/Non-Volatile Writable

The WPS bit is used to select which Write Protect scheme should be used. When WPS=0, the device will
use the combination of CMP, SEC, TB, BP[2:0] bits to protect a specific area of the memory array. When
WPS=1, the device will utilize the Individual Block Locks to protect any individual sector or blocks. The
default value for all Individual Block Lock bits is 1 upon device power on or after reset.

#### 7.1.6 Output Driver Strength (DRV1, DRV0) – Volatile/Non-Volatile Writable

The DRV1 & DRV0 bits are used to determine the output driver strength for the Read operations.

```
DRV1, DRV0 Driver Strength
```
```
0, 0 100%
```
```
0, 1 75%(1)
```
```
1, 0 50%
```
```
1, 1 25%(2)^
Notes:
```
1. Factory default for part numbers with ordering options “IN”
2. Factory default for part numbers with ordering options “IQ”.

#### 7.1.7 Reserved Bits – Non Functional

There are a few reserved Status Register bits that may be read out as a “0” or “1”. It is recommended to
ignore the values of those bits. During a “Write Status Register” instruction, the Reserved Bits can be
written as “0”, but there will not be any effects.


```
Publication Release Date: December 24, 2024
```
- 18 - Revision M

#### 7.1.8 W25Q128JV Status Register Memory Protection (WPS = 0, CMP = 0)

```
STATUS REGISTER(1) W25Q128JV (128M-BIT) MEMORY PROTECTION(3)
```
SEC TB BP2 BP1 BP

```
PROTECTED
BLOCK(S)
```
```
PROTECTED
ADDRESSES
```
```
PROTECTED
DENSITY
```
```
PROTECTED
PORTION(2)
```
```
X X 0 0 0 NONE NONE NONE NONE
```
```
0 0 0 0 1 252 thru 255 FC0000h – FFFFFFh 256KB Upper 1/
```
```
0 0 0 1 0 248 thru 255 F80000h – FFFFFFh 512KB Upper 1/
```
```
0 0 0 1 1 240 thru 255 F0 0000 h – FFFFFFh 1MB Upper 1/
```
```
0 0 1 0 0 224 thru 255 E00000h – FFFFFFh 2MB Upper 1/
```
```
0 0 1 0 1 192 thru 255 C00000h – FFFFFFh 4MB Upper 1/
```
```
0 0 1 1 0 128 thru 255 8 00000h – FFFFFFh 8MB Upper 1/
```
```
0 1 0 0 1 0 thru 3 000000h – 03FFFFh 256KB Lower 1/
```
```
0 1 0 1 0 0 thru 7 000000h – 07FFFFh 512KB Lower 1/
```
```
0 1 0 1 1 0 thru 15 000000h – 0FFFFFh 1MB Lower 1/
```
```
0 1 1 0 0 0 thru 31 000000h – 1FFFFFh 2MB Lower 1/
```
```
0 1 1 0 1 0 thru 63 000000h – 3FFFFFh 4MB Lower 1/
```
```
0 1 1 1 0 0 thru 127 000000h – 7 FFFFFh 8MB Lower 1/
```
```
X X 1 1 1 0 thru 255 000000h – FFFFFFh 16MB ALL
```
```
1 0 0 0 1 255 FFF000h – FFFFFFh 4KB U - 1/
```
```
1 0 0 1 0 255 FFE000h – FFFFFFh 8KB U - 1/
```
```
1 0 0 1 1 255 FFC000h – FFFFFFh 16KB U - 1/
```
```
1 0 1 0 X 255 FF8000h – FFFFFFh 32KB U - 1/
```
```
1 1 0 0 1 0 000000h – 000FFFh 4KB L - 1/
```
```
1 1 0 1 0 0 000000h – 001FFFh 8KB L - 1/
```
```
1 1 0 1 1 0 000000h – 003FFFh 1 6KB L - 1/
```
```
1 1 1 0 X 0 000000h – 007FFFh 32KB L - 1/
```
```
Notes:
```
3. X = don’t care
4. L = Lower; U = Upper
5. If any Erase or Program command specifies a memory region that contains protected data portion, this
    command will be ignored.


##### - 19 -

#### 7.1.9 W25Q128JV Status Register Memory Protection (WPS = 0, CMP = 1)

```
STATUS REGISTER(1) W25Q128JV (128M-BIT) MEMORY PROTECTION(3)
```
SEC TB BP2 BP1 BP

```
PROTECTED
BLOCK(S)
```
```
PROTECTED
ADDRESSES
```
```
PROTECTED
DENSITY
```
```
PROTECTED
PORTION(2)
```
```
X X 0 0 0 0 thru 255 000000h - FFFFFFh 16MB ALL
```
```
0 0 0 0 1 0 thru 251 000000h - FBFFFFh 16 ,128KB Lower 63/
```
```
0 0 0 1 0 0 thru 247 000000h – F7FFFFh 15,872KB Lower 31/
```
```
0 0 0 1 1 0 thru 2 39 000000h - EFFFFFh 15 MB Lower 15/
```
```
0 0 1 0 0 0 thru 223 000000h - DFFFFFh 14MB Lower 7/
```
```
0 0 1 0 1 0 thru 191 00000 0h - BFFFFFh 12 MB Lower 3/
```
```
0 0 1 1 0 0 thru 127 000000h - 7FFFFFh 8 MB Lower 1/
```
```
0 1 0 0 1 4 thru 255 040000h - FFFFFFh 16 ,128KB Upper 63/
```
```
0 1 0 1 0 8 thru 255 080000h - FFFFFFh 15,872KB Upper 31/
```
```
0 1 0 1 1 16 thru 255 100000h - FFFFFFh 15 MB Upper 15/
```
```
0 1 1 0 0 32 thru 255 200000h - FFFFFFh 14MB Upper 7/
```
```
0 1 1 0 1 64 thru 255 400000h - FFFFFFh 12 MB Upper 3/
```
```
0 1 1 1 0 128 thru 255 800000h - FFFFFFh 8 MB Upper 1/
```
```
X X 1 1 1 NONE NONE NONE NONE
```
```
1 0 0 0 1 0 thru 255 000000 h – FFEFFFh 16 ,380KB L - 4095/
```
```
1 0 0 1 0 0 thru 255 000000 h – FFDFFFh 16 ,376KB L - 2047/
```
```
1 0 0 1 1 0 thru 255 000000 h – FFBFFFh 16 ,368KB L - 1023/
```
```
1 0 1 0 X 0 thru 255 000000 h – FF7FFFh 16 ,352KB L - 511/
```
```
1 1 0 0 1 0 thru 255 001 000h – FFFFFFh 16 ,380KB U - 409 5/
```
```
1 1 0 1 0 0 thru 255 002 000h – FFFFFFh 16 ,376KB U - 2047/
```
```
1 1 0 1 1 0 thru 255 004 000h – FFFFFFh 16 ,368KB U -1023/
```
```
1 1 1 0 X 0 thru 255 008 000h – FFFFFFh 16 ,352KB U - 511/
```
```
Notes:
```
1. X = don’t care
2. L = Lower; U = Upper
3. If any Erase or Program command specifies a memory region that contains protected data portion, this
    command will be ignored.


```
Publication Release Date: December 24, 2024
```
- 20 - Revision M

#### 7.1.10 W25Q128JV Individual Block Memory Protection (WPS=1)

```
Sector 0 ( 4 KB)
```
```
Sector 1 ( 4 KB)
```
```
Sector 14 ( 4 KB)
```
```
Sector 15 ( 4 KB)
```
```
Block 1 ( 64 KB)
```
```
Block 254 ( 64 KB)
```
```
Sector 0 ( 4 KB)
```
```
Sector 1 ( 4 KB)
```
```
Sector 14 ( 4 KB)
```
```
Sector 15 ( 4 KB)
```
```
B
```
```
lo
```
```
ck
```
```
0
```
```
(^6
```
```
4 K
```
```
B
```
```
)
```
```
B
```
```
lo
```
```
ck
```
```
2
```
```
55
```
```
(^6
```
```
4 K
```
```
B
```
```
)
```
```
Individual Block Locks:
32 Sectors (Top/Bottom)
254 Blocks
```
```
Individual Block Lock:
36 h + Address
```
```
Individual Block Unlock:
39 h + Address
```
```
Read Block Lock:
3 Dh + Address
```
```
Global Block Lock:
7 Eh
```
```
Global Block Unlock:
98 h
```
```
Figure 4d. Individual Block/Sector Locks
```
Notes:

1. Individual Block/Sector protection is only valid when WPS=1.
2. All individual block/sector lock bits are set to 1 by default after power up, all memory array is protected.


##### - 21 -

## 8. INSTRUCTIONS

The Standard/Dual/Quad SPI instruction set of the W25Q128JV consists of 47 basic instructions that are
fully controlled through the SPI bus (see Instruction Set Table 1 - 2 ). Instructions are initiated with the falling
edge of Chip Select (/CS). The first byte of data clocked into the DI input provides the instruction code.
Data on the DI input is sampled on the rising edge of clock with most significant bit (MSB) first.

Instructions vary in length from a single byte to several bytes and may be followed by address bytes, data
bytes, dummy bytes (don’t care), and in some cases, a combination. Instructions are completed with the
rising edge of edge /CS. Clock relative timing diagrams for each instruction are included in Figures 5
through 57. All read instructions can be completed after any clocked bit. However, all instructions that
Write, Program or Erase must complete on a byte boundary (/CS driven high after a full 8-bits have been
clocked) otherwise the instruction will be ignored. This feature further protects the device from inadvertent
writes. Additionally, while the memory is being programmed or erased, or when the Status Register is
being written, all instructions except for Read Status Register will be ignored until the program or erase
cycle has completed.

### 8.1 Device ID and Instruction Set Tables

#### 8.1.1 Manufacturer and Device Identification

```
MANUFACTURER ID (MF 7 - MF0)
```
```
Winbond Serial Flash EFh
```
```
Device ID (ID7 - ID0) (ID15 - ID0)
```
```
Instruction ABh, 90h, 92h, 94h 9Fh^
```
```
W25Q128JV-IN/IQ/JQ 17 h 4018 h
```
```
W25Q128JV-IM*/JM* 17 h 7018 h
```
```
Note: For DTR, QPI supporting, please refer to W25Q 128 JV-M DTR datasheet.
```

```
Publication Release Date: December 24, 2024
```
- 22 - Revision M

#### 8.1.2 Instruction Set Table 1 (Standard SPI Instructions)(1)

Data Input Output Byte 1 Byte 2 Byte 3 Byte 4 Byte 5 Byte 6 Byte 7

Number of Clock(1- 1 - 1) 8 8 8 8 8 8 8

Write Enable 06h

Volatile SR Write Enable 50h
Write Disable 04h

Release Power-down / ID ABh Dummy Dummy Dummy (ID7-ID0)(2)
Manufacturer/Device ID 90h Dummy Dummy 00h (MF7-MF0) (ID7-ID0)

JEDEC ID 9Fh (MF7-MF0) (ID15-ID8) (ID7-ID0)
Read Unique ID 4Bh Dummy Dummy Dummy Dummy (UID63- 0 )

Read Data 03h A23-A16 A15-A8 A7-A0 (D7-D0)

Fast Read 0Bh A23-A16 A15-A8 A7-A0 Dummy (D7-D0)

Page Program 02h A23-A16 A15-A8 A7-A0 D7-D0 D7-D0(3)

Sector Erase (4KB) 20h A23-A16 A15-A8 A7-A0
Block Erase (32KB) 52h A23-A16 A15-A8 A7-A0

Block Erase (64KB) D8h A23-A16 A15-A8 A7-A0
Chip Erase C7h/60h

Read Status Register- 1 05h (S7-S0)(2)
Write Status Register- 1 (4) 01h (S7-S0)(4)

Read Status Register- 2 35h (S15-S8)(2)
Write Status Register- 2 31h (S15-S8)

Read Status Register- 3 15h (S23-S16)(2)
Write Status Register- 3 11h (S23-S16)

Read SFDP Register 5Ah 00 00 A7-A0 Dummy (D7-D0)

Erase Security Register(5) 44h A23-A16 A15-A8 A7-A0
Program Security Register(5) 42h A23-A16 A15-A8 A7-A0 D7-D0 D7-D0(3)

Read Security Register(5) 48h A23-A16 A15-A8 A7-A0 Dummy (D7-D0)

Global Block Lock 7Eh

Global Block Unlock 98h
Read Block Lock 3Dh A23-A16 A15-A8 A7-A0 (L7-L0)

Individual Block Lock 36h A23-A16 A15-A8 A7-A0
Individual Block Unlock 39h A23-A16 A15-A8 A7-A0

Erase / Program Suspend 75h
Erase / Program Resume 7Ah

Power-down B9h

Enable Reset 66h

Reset Device 99h


##### - 23 -

#### 8.1.3 Instruction Set Table 2 (Dual/Quad SPI Instructions)

Data Input Output Byte 1 Byte 2 Byte 3 Byte 4 Byte 5 Byte 6 Byte 7 Byte 8 Byte 9
Number of Clock(1- 1 - 2) 8 8 8 8 4 4 4 4 4
Fast Read Dual Output 3Bh A23-A16 A15-A8 A7-A0 Dummy Dummy (D7-D0)(7)

Number of Clock(1- 2 - 2) 8 4 4 4 4 4 4 4 4

Fast Read Dual I/O BBh A23-A16(6) A15-A8(6) A7-A0(6) Dummy(11) (D7-D0)(7)

Mftr./Device ID Dual I/O 92h A23-A16(6) A15-A8(6) 00 (6) Dummy(11) (MF7-MF0) (ID7-ID0)(7)

Number of Clock( 1 - 1 - 4) 8 8 8 8 2 2 2 2 2

Quad Input Page Program 32h A23-A16 A15-A8 A7-A0 (D7-D0)(^9 ) (D7-D0)(^3 ) ...

Fast Read Quad Output 6Bh A23-A16 A15-A8 A7-A0 Dummy Dummy Dummy Dummy (D7-D0)(^10 )

Number of Clock(1- 4 - 4 ) 8 2 (8) 2 (8) 2 (8) 2 2 2 2 2

Mftr./Device ID Quad I/O 94h A23-A16 A15-A8 00 Dummy(11) Dummy Dummy (MF7-MF0) (ID7-ID0)

Fast Read Quad I/O EBh A23-A16 A15-A8 A7-A0 Dummy(11) Dummy Dummy (D7-D0)

Set Burst with Wrap 77h^ Dummy Dummy Dummy (^) W8-W0

#### Notes:

1. Data bytes are shifted with Most Significant Bit first. Byte fields with data in parenthesis “( )” indicate data
    output from the device on either 1, 2 or 4 IO pins.
2. The Status Register contents and Device ID will repeat continuously until /CS terminates the instruction.
3. At least one byte of data input is required for Page Program, Quad Page Program and Program Security
    Registers, up to 256 bytes of data input. If more than 256 bytes of data are sent to the device, the
    addressing will wrap to the beginning of the page and overwrite previously sent data.
4. Write Status Register-1 (01h) can also be used to program Status Register-1&2, see section 8.2.5.
5. Security Register Address:
    Security Register 1: A23-16 = 00h; A15-8 = 10h; A7-0 = byte address
    Security Register 2: A23-16 = 00h; A15-8 = 20h; A7-0 = byte address
    Security Register 3: A23-16 = 00h; A15-8 = 30h; A7-0 = byte address
6. Dual SPI address input format:
    IO0 = A22, A20, A18, A16, A14, A12, A10, A8 A6, A4, A2, A0, M6, M4, M2, M0
    IO1 = A23, A21, A19, A17, A15, A13, A11, A9 A7, A5, A3, A1, M7, M5, M3, M1
7. Dual SPI data output format:
    IO0 = (D6, D4, D2, D0)
    IO1 = (D7, D5, D3, D1)
8. Quad SPI address input format: Set Burst with Wrap input format:
    IO0 = A20, A16, A12, A8, A4, A0, M4, M0 IO0 = x, x, x, x, x, x, W4, x
    IO1 = A21, A17, A13, A9, A5, A1, M5, M1 IO1 = x, x, x, x, x, x, W5, x
    IO2 = A22, A18, A14, A10, A6, A2, M6, M2 IO2 = x, x, x, x, x, x, W6, x
    IO3 = A23, A19, A15, A11, A7, A3, M7, M3 IO3 = x, x, x, x, x, x, x, x
9. Quad SPI data input/output format:
    IO0 = (D4, D0, .....)
    IO1 = (D5, D1, .....)
    IO2 = (D6, D2, .....)
    IO3 = (D7, D3, .....)
10. Fast Read Quad I/O data output format:
IO0 = (x, x, x, x, D4, D0, D4, D0)
IO1 = (x, x, x, x, D5, D1, D5, D1)
IO2 = (x, x, x, x, D6, D2, D6, D2)
IO3 = (x, x, x, x, D7, D3, D7, D3)
11. The first dummy is M7-M0 should be set to Fxh


```
Publication Release Date: December 24, 2024
```
- 24 - Revision M

### 8.2 Instruction Descriptions

#### 8.2.1 Write Enable (06h)

The Write Enable instruction (Figure 5) sets the Write Enable Latch (WEL) bit in the Status Register to a

1. The WEL bit must be set prior to every Page Program, Quad Page Program, Sector Erase, Block
Erase, Chip Erase, Write Status Register and Erase/Program Security Registers instruction. The Write
Enable instruction is entered by driving /CS low, shifting the instruction code “06h” into the Data Input (DI)
pin on the rising edge of CLK, and then driving /CS high.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
Mode 0
```
```
Mode 3
```
```
Instruction (06h)
```
```
High Impedance
```
```
Figure 5. Write Enable Instruction for SPI Mode
```
#### 8.2.2 Write Enable for Volatile Status Register (50h)....................................................................

The non-volatile Status Register bits described in section 7.1 can also be written to as volatile bits. This
gives more flexibility to change the system configuration and memory protection schemes quickly without
waiting for the typical non-volatile bit write cycles or affecting the endurance of the Status Register non-
volatile bits. To write the volatile values into the Status Register bits, the Write Enable for Volatile Status
Register (50h) instruction must be issued prior to a Write Status Register (01h) instruction. Write Enable
for Volatile Status Register instruction (Figure 6) will not set the Write Enable Latch (WEL) bit, it is only
valid for the Write Status Register instruction to change the volatile Status Register bit values.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
Mode 0
```
```
Mode 3
```
```
Instruction (50h)
```
```
High Impedance
```
```
Figure 6. Write Enable for Volatile Status Register Instruction for SPI Mode
```

##### - 25 -

#### 8.2.3 Write Disable (04h)

The Write Disable instruction (Figure 7) resets the Write Enable Latch (WEL) bit in the Status Register to
a 0. The Write Disable instruction is entered by driving /CS low, shifting the instruction code “04h” into the
DI pin and then driving /CS high. Note that the WEL bit is automatically reset after Power-up and upon
completion of the Write Status Register, Erase/Program Security Registers, Page Program, Quad Page
Program, Sector Erase, Block Erase, Chip Erase and Reset instructions.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
Mode 0
```
```
Mode 3
```
```
Instruction (04h)
```
```
High Impedance
```
Figure 7. Write Disable Instruction for SPI Mode

#### 8.2.4 Read Status Register-1 (05h), Status Register- 2 ( 3 5h) & Status Register-3 (15h)

The Read Status Register instructions allow the 8-bit Status Registers to be read. The instruction is
entered by driving /CS low and shifting the instruction code “05h” for Status Register-1, “35h” for Status
Register-2 or “15h” for Status Register- 3 into the DI pin on the rising edge of CLK. The status register bits
are then shifted out on the DO pin at the falling edge of CLK with most significant bit (MSB) first as shown
in Figure 8. Refer to section 7.1 for Status Register descriptions.

The Read Status Register instruction may be used at any time, even while a Program, Erase or Write
Status Register cycle is in progress. This allows the BUSY status bit to be checked to determine when the
cycle is complete and if the device can accept another instruction. The Status Register can be read
continuously, as shown in Figure 8. The instruction is completed by driving /CS high.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction ( 05 h/ 35 h/ 15 h)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
```
```
7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7
```
```
Status Register- 1 / 2 / 3 out Status Register- 1 / 2 / 3 out
```
## *=^ MSB * *

```
Figure 8a. Read Status Register Instruction
```

```
Publication Release Date: December 24, 2024
```
- 26 - Revision M

#### 8.2.5 Write Status Register-1 (01h), Status Register- 2 ( 3 1h) & Status Register-3 (11h)

The Write Status Register instruction allows the Status Registers to be written. The writable Status
Register bits include: SEC, TB, BP[2:0] in Status Register-1; CMP, LB[3:1], QE, SRL in Status Register-2;
DRV1, DRV0, WPS in Status Register-3. All other Status Register bit locations are read-only and will not
be affected by the Write Status Register instruction. LB[3:1] are non-volatile OTP bits, once it is set to 1, it
cannot be cleared to 0.

To write non-volatile Status Register bits, a standard Write Enable (06h) instruction must previously have
been executed for the device to accept the Write Status Register instruction (Status Register bit WEL
must equal 1). Once write enabled, the instruction is entered by driving /CS low, sending the instruction
code “01h/31h/11h”, and then writing the status register data byte as illustrated in Figure 9a.

To write volatile Status Register bits, a Write Enable for Volatile Status Register (50h) instruction must
have been executed prior to the Write Status Register instruction (Status Register bit WEL remains 0).
However, SRL and LB[3:1] cannot be changed from “1” to “0” because of the OTP protection for these
bits. Upon power off or the execution of a Software/Hardware Reset, the volatile Status Register bit values
will be lost, and the non-volatile Status Register bit values will be restored.

During non-volatile Status Register write operation (06h combined with 01h/31h/11h), after /CS is driven
high, the self-timed Write Status Register cycle will commence for a time duration of tW (See AC
Characteristics). While the Write Status Register cycle is in progress, the Read Status Register instruction
may still be accessed to check the status of the BUSY bit. The BUSY bit is a 1 during the Write Status
Register cycle and a 0 when the cycle is finished and ready to accept other instructions again. After the
Write Status Register cycle has finished, the Write Enable Latch (WEL) bit in the Status Register will be
cleared to 0.

During volatile Status Register write operation (50h combined with 01h/31h/11h), after /CS is driven high,
the Status Register bits will be refreshed to the new values within the time period of tSHSL2 (See AC
Characteristics). BUSY bit will remain 0 during the Status Register bit refresh period.

Refer to section 7.1 for Status Register descriptions.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction
( 01 h/ 31 h/ 11 h)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15
```
```
7 6 5 4 3 2 1 0
```
```
Register- 1 / 2 / 3 in
```
```
Mode 0
```
```
Mode 3
```
*

*=^ MSB

```
Figure 9a. Write Status Register-1/2/3 Instruction^
```

##### - 27 -

The W25Q128JV is also backward compatible to Winbond’s previous generations of serial flash
memories, in which the Status Register-1&2 can be written using a single “Write Status Register-1 (01h)”
command. To complete the Write Status Register-1&2 instruction, the /CS pin must be driven high after
the sixteenth bit of data that is clocked in as shown in Figure 9b. If /CS is driven high after the eighth
clock, the Write Status Register-1 (01h) instruction will only program the Status Register-1, the Status
Register-2 will not be affected (Previous generations will clear CMP and QE bits).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (01h)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
```
```
7 6 5 4 3 2 1 0 15 14 13 12 11 10 9 8
```
```
Status Register 1 in Status Register 2 in
```
```
Mode 0
```
```
Mode 3
```
## * *

## *= MSB

```
Figure 9b. Write Status Register-1/2 Instruction
```

```
Publication Release Date: December 24, 2024
```
- 28 - Revision M

#### 8.2.6 Read Data (03h)

The Read Data instruction allows one or more data bytes to be sequentially read from the memory. The
instruction is initiated by driving the /CS pin low and then shifting the instruction code “03h” followed by a
24 - bit address (A23-A0) into the DI pin. The code and address bits are latched on the rising edge of the
CLK pin. After the address is received, the data byte of the addressed memory location will be shifted out
on the DO pin at the falling edge of CLK with most significant bit (MSB) first. The address is automatically
incremented to the next higher address after each byte of data is shifted out allowing for a continuous
stream of data. This means that the entire memory can be accessed with a single instruction as long as
the clock continues. The instruction is completed by driving /CS high.

The Read Data instruction sequence is shown in Figure 14. If a Read Data instruction is issued while an
Erase, Program or Write cycle is in process (BUSY=1) the instruction is ignored and will not have any
effects on the current cycle. The Read Data instruction allows clock rates from D.C. to a maximum of fR
(see AC Electrical Characteristics).

The Read Data (03h) instruction is only supported in Standard SPI mode.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (03h)
```
```
High Impedance
```
```
8 9 10 28 29 30 31 32 33 34 35 36 37 38 39
```
```
7 6 5 4 3 2 1 0 7
```
```
24-Bit Address
23 22 21 3 2 1 0
```
```
Data Out 1
```
## *

## *

## *= MSB

```
Figure 14. Read Data Instruction
```

##### - 29 -

#### 8.2.7 Fast Read (0Bh)

The Fast Read instruction is similar to the Read Data instruction except that it can operate at the highest
possible frequency of FR (see AC Electrical Characteristics). This is accomplished by adding eight
“dummy” clocks after the 24-bit address as shown in Figure 16. The dummy clocks allow the devices
internal circuits additional time for setting up the initial address. During the dummy clocks the data value
on the DO pin is a “don’t care”.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (0Bh)
```
```
High Impedance
```
```
8 9 10 28 29 30 31
```
```
24-Bit Address
23 22 21 3 2 1 0
```
```
Data Out 1
```
## *

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
32 33 34 35 36 37 38 39
```
```
Dummy Clocks
```
```
High Impedance
```
```
40 41 42 44 45 46 47 48 49 50 51 52 53 54 55
```
```
7 6 5 4 3 2 1 0 7
```
```
Data Out 2
```
## *

```
7 6 5 4 3 2 1 0
```
## *

```
31 43
```
```
0
```
## *= MSB

```
Figure 16a. Fast Read Instruction
```

```
Publication Release Date: December 24, 2024
```
- 30 - Revision M

#### 8.2.8 Fast Read Dual Output (3Bh)

The Fast Read Dual Output (3Bh) instruction is similar to the standard Fast Read (0Bh) instruction except
that data is output on two pins; IO 0 and IO 1. This allows data to be transferred at twice the rate of standard
SPI devices. The Fast Read Dual Output instruction is ideal for quickly downloading code from Flash to
RAM upon power-up or for applications that cache code-segments to RAM for execution.

Similar to the Fast Read instruction, the Fast Read Dual Output instruction can operate at the highest
possible frequency of FR (see AC Electrical Characteristics). This is accomplished by adding eight
“dummy” clocks after the 24-bit address as shown in Figure 18. The dummy clocks allow the device's
internal circuits additional time for setting up the initial address. The input data during the dummy clocks is
“don’t care”. However, the IO 0 pin should be high-impedance prior to the falling edge of the first data out
clock.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (3Bh)
```
```
High Impedance
```
```
8 9 10 28 29 30
```
```
32 33 34 35 36 37 38 39
```
```
6 4 2 0
```
```
24-Bit Address
23 22 21 3 2 1 0
```
*

*

```
31
```
```
31
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Dummy Clocks
0
```
```
40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55
```
```
7 5 3 1
```
```
High Impedance
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
IO 0 switches from
Input to Output
6
```
```
7
Data Out 1 *Data Out 2 *Data Out 3 *Data Out 4
```
*= MSB

```
Figure 18. Fast Read Dual Output Instruction
```

##### - 31 -

#### 8.2.9 Fast Read Quad Output (6Bh)

The Fast Read Quad Output (6Bh) instruction is similar to the Fast Read Dual Output (3Bh) instruction
except that data is output on four pins, IO 0 , IO 1 , IO 2 , and IO 3. The Quad Enable (QE) bit in Status
Register- 2 must be set to 1 before the device will accept the Fast Read Quad Output Instruction. The Fast
Read Quad Output Instruction allows data to be transferred at four times the rate of standard SPI devices.

The Fast Read Quad Output instruction can operate at the highest possible frequency of FR (see AC
Electrical Characteristics). This is accomplished by adding eight “dummy” clocks after the 24-bit address
as shown in Figure 20. The dummy clocks allow the device's internal circuits additional time for setting up
the initial address. The input data during the dummy clocks is “don’t care”. However, the IO pins should be
high-impedance prior to the falling edge of the first data out clock.

```
/CS
```
```
CLK Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (6Bh)
```
```
High Impedance
```
```
8 9 10 28 29 30
```
```
32 33 34 35 36 37 38 39
```
```
4 0
```
```
24-Bit Address
```
```
23 22 21 3 2 1 0
```
*

```
31
```
```
31
```
```
/CS
```
```
CLK
```
```
Dummy Clocks
```
```
0
```
```
40 41 42 43 44 45 46 47
```
```
5 1
```
```
High Impedance
```
```
4
```
```
5
```
```
Byte 1
```
```
High Impedance
```
```
High Impedance
```
```
6 2
```
```
7 3
```
```
High Impedance
```
```
6
```
```
7
```
```
High Impedance
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
Byte 2 Byte 3 Byte 4
```
```
IO 0 switches from
Input to Output
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
*= MSB

```
Figure 20. Fast Read Quad Output Instruction
```

```
Publication Release Date: December 24, 2024
```
- 32 - Revision M

#### 8.2.10 Fast Read Dual I/O (BBh)

The Fast Read Dual I/O (BBh) instruction allows for improved random access while maintaining two IO
pins, IO 0 and IO 1. It is similar to the Fast Read Dual Output (3Bh) instruction but with the capability to input
the Address bits (A23-0) two bits per clock. This reduced instruction overhead may allow for code
execution (XIP) directly from the Dual SPI in some applications.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (BBh)
```
```
8 9 10 12 13 14
```
```
24 25 26 27 28 29 30 31
```
```
6 4 2 0
```
## *

## *

```
23
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
0
```
```
32 33 34 35 36 37 38 39
```
```
7 5 3 1
```
## *

```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
## * *

```
IOs switch from
Input to Output
6
```
```
7
```
```
22 20 18 16
```
```
23 21 19 17
```
```
14 12 10 8
```
```
15 13 11 9
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
11 15 16 17 18 19 20 21 22 23
```
```
1
```
```
A23-16 A15-8 A7-0 M7-0
```
```
Byte 1 Byte 2 Byte 3 Byte 4
```
## *= MSB *

```
Figure 22a. Fast Read Dual I/O Instruction (M7-M0 should be set to Fxh)
```

##### - 33 -

#### 8.2.11 Fast Read Quad I/O (EBh)

The Fast Read Quad I/O (EBh) instruction is similar to the Fast Read Dual I/O (BBh) instruction except
that address and data bits are input and output through four pins IO 0 , IO 1 , IO 2 and IO 3 and four Dummy
clocks are required in SPI mode prior to the data output. The Quad I/O dramatically reduces instruction
overhead allowing faster random access for code execution (XIP) directly from the Quad SPI. The Quad
Enable bit (QE) of Status Register- 2 must be set to enable the Fast Read Quad I/O Instruction.

```
Figure 24a. Fast Read Quad I/O Instruction (M7-M0 should be set to Fxh)
```
```
M7-0
```
```
/CS
```
```
CLK Mode 0
```
```
Mode 3 0 1
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
2 3 4 5
```
```
20 16 12 8
```
```
21 17
```
```
22 18
```
```
23 19
```
```
13 9
```
```
14 10
```
```
15 11
```
```
A23-16
```
```
6 7 8 9
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
A15-8 A7-0
```
```
Byte 1 Byte 2
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
10 11 12 13 14
```
```
4
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
IOs switch from
Input to Output
```
```
Byte 3
```
```
15 16 17 18 19 20 21 22 23
```
```
Instruction (EBh) Dummy Dummy
```

```
Publication Release Date: December 24, 2024
```
- 34 - Revision M

#### Fast Read Quad I/O with “8/16/32/64 - Byte Wrap Around” in Standard SPI mode

The Fast Read Quad I/O instruction can also be used to access a specific portion within a page by issuing
a “Set Burst with Wrap” (77h) command prior to EBh. The “Set Burst with Wrap” (77h) command can
either enable or disable the “Wrap Around” feature for the following EBh commands. When “Wrap
Around” is enabled, the data being accessed can be limited to either an 8, 16, 32 or 64-byte section of a
256 - byte page. The output data starts at the initial address specified in the instruction, once it reaches the
ending boundary of the 8/16/32/64-byte section, the output will wrap around to the beginning boundary
automatically until /CS is pulled high to terminate the command.

The Burst with Wrap feature allows applications that use cache to quickly fetch a critical address and then
fill the cache afterwards within a fixed length (8/16/32/64-byte) of data without issuing multiple read
commands.

The “Set Burst with Wrap” instruction allows three “Wrap Bits”, W6-4 to be set. The W4 bit is used to
enable or disable the “Wrap Around” operation while W6-5 are used to specify the length of the wrap
around section within a page. Refer to section 8.2.37 for detail descriptions.


##### - 35 -

#### 8.2.12 Set Burst with Wrap (77h)

In Standard SPI mode, the Set Burst with Wrap (77h) instruction is used in conjunction with “Fast Read
Quad I/O” instruction to access a fixed length of 8/16/32/64-byte section within a 256-byte page. Certain
applications can benefit from this feature and improve the overall system code execution performance.

Similar to a Quad I/O instruction, the Set Burst with Wrap instruction is initiated by driving the /CS pin low
and then shifting the instruction code “77h” followed by 24 dummy bits and 8 “Wrap Bits”, W7-0. The
instruction sequence is shown in Figure 28. Wrap bit W7 and the lower nibble W3-0 are not used.

```
W6, W5
```
```
W4 = 0 W4 =1 (DEFAULT)
Wrap Around Wrap Length Wrap Around Wrap Length
0 0 Yes 8 - byte No N/A
0 1 Yes 16 - byte No N/A
1 0 Yes^32 - byte^ No^ N/A^
1 1 Yes 64 - byte No N/A
```
Once W6-4 is set by a Set Burst with Wrap instruction, all the following “Fast Read Quad I/O” instruction
will use the W6-4 setting to access the 8/16/32/64-byte section within any page. To exit the “Wrap Around”
function and return to normal read operation, another Set Burst with Wrap instruction should be issued to
set W4 = 1. The default value of W4 upon power on or after a software/hardware reset is 1.

```
Wrap Bit
```
```
/CS
```
```
CLK Mode 0
```
```
Mode 3 0 1
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
2 3 4 5
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
don't
care
```
```
6 7 8 9
```
```
don't
care
```
```
don't
care
```
```
10 11 12 13 14 15
```
```
Instruction (77h)
```
```
Mode 0
```
```
Mode 3
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
X X
```
```
w4 X
```
```
w5 X
```
```
w6 X
```
```
X X
```
```
Figure 28. Set Burst with Wrap Instruction
```

```
Publication Release Date: December 24, 2024
```
- 36 - Revision M

#### 8.2.13 Page Program (02h)

The Page Program instruction allows from one byte to 256 bytes (a page) of data to be programmed at
previously erased (FFh) memory locations. A Write Enable instruction must be executed before the device
will accept the Page Program Instruction (Status Register bit WEL= 1). The instruction is initiated by
driving the /CS pin low then shifting the instruction code “02h” followed by a 24-bit address (A23-A0) and
at least one data byte, into the DI pin. The /CS pin must be held low for the entire length of the instruction
while data is being sent to the device. The Page Program instruction sequence is shown in Figure 29.

If an entire 256 byte page is to be programmed, the last address byte (the 8 least significant address bits)
should be set to 0. If the last address byte is not zero, and the number of clocks exceeds the remaining
page length, the addressing will wrap to the beginning of the page. In some cases, less than 256 bytes (a
partial page) can be programmed without having any effect on other bytes within the same page. One
condition to perform a partial page program is that the number of clocks cannot exceed the remaining
page length. If more than 256 bytes are sent to the device the addressing will wrap to the beginning of the
page and overwrite previously sent data.

As with the write and erase instructions, the /CS pin must be driven high after the eighth bit of the last byte
has been latched. If this is not done the Page Program instruction will not be executed. After /CS is driven
high, the self-timed Page Program instruction will commence for a time duration of tpp (See AC
Characteristics). While the Page Program cycle is in progress, the Read Status Register instruction may
still be accessed for checking the status of the BUSY bit. The BUSY bit is a 1 during the Page Program
cycle and becomes a 0 when the cycle is finished and the device is ready to accept other instructions
again. After the Page Program cycle has finished the Write Enable Latch (WEL) bit in the Status Register
is cleared to 0. The Page Program instruction will not be executed if the addressed page is protected by
the Block Protect (CMP, SEC, TB, BP2, BP1, and BP0) bits or the Individual Block/Sector Locks.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (02h)
```
```
8 9 10 28 29 30 39
```
```
24-Bit Address
23 22 21 3 2 1
```
## *

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
40 41 42 43 44 45 46 47
```
```
Data Byte 2
```
```
48 49 50 52 53 54 55 2072
```
```
7 6 5 4 3 2 1 0
```
```
39 51
```
```
0
```
```
31
```
```
0
```
```
32 33 34 35 36 37 38
```
```
Data Byte 1
7 6 5 4 3 2 1
```
## *

```
Mode 0
```
```
Mode 3
```
```
Data Byte 3
```
```
2073207420752076207720782079
```
```
0
```
```
Data Byte 256
```
## *

```
7 6 5 4 3 2 1 0
```
## *

```
7 6 5 4 3 2 1 0
```
## *

## *= MSB

```
Figure 29a. Page Program Instruction
```

##### - 37 -

#### 8.2.14 Quad Input Page Program ( 3 2h)

The Quad Page Program instruction allows up to 256 bytes of data to be programmed at previously
erased (FFh) memory locations using four pins: IO 0 , IO 1 , IO 2 , and IO 3. The Quad Page Program can
improve performance for PROM Programmer and applications that have slow clock speeds <5MHz.
Systems with faster clock speed will not realize much benefit for the Quad Page Program instruction since
the inherent page program time is much greater than the time it take to clock-in the data.

To use Quad Page Program the Quad Enable (QE) bit in Status Register-2 must be set to 1. A Write
Enable instruction must be executed before the device will accept the Quad Page Program instruction
(Status Register-1, WEL=1). The instruction is initiated by driving the /CS pin low then shifting the
instruction code “32h” followed by a 24-bit address (A23-A0) and at least one data byte, into the IO pins.
The /CS pin must be held low for the entire length of the instruction while data is being sent to the device.
All other functions of Quad Page Program are identical to standard Page Program. The Quad Page
Program instruction sequence is shown in Figure 30.

```
/CS
```
```
CLK Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (32h)
```
```
8 9 10 28 29 30
```
```
32 33 34 35 36 37
```
```
4 0
```
```
24-Bit Address
```
```
23 22 21 3 2 1 0
```
*

```
31
```
```
31
```
```
/CS
```
```
CLK
```
```
5 1
```
```
Byte 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
Byte 2 Byte 3 Byte 256
0 4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
4 0
```
```
5 1
```
```
6 2
```
```
7 3
```
```
536 537 538 539 540 541 542 543
Mode 0
```
```
Mode 3
```
```
Byte
253
```
```
Byte
254
```
```
Byte
255
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
* * * * * * *

```
= MSB
*
```
```
Figure 30. Quad Input Page Program Instruction
```

```
Publication Release Date: December 24, 2024
```
- 38 - Revision M

#### 8.2.15 Sector Erase (20h)

The Sector Erase instruction sets all memory within a specified sector (4K-bytes) to the erased state of all
1s (FFh). A Write Enable instruction must be executed before the device will accept the Sector Erase
Instruction (Status Register bit WEL must equal 1). The instruction is initiated by driving the /CS pin low
and shifting the instruction code “20h” followed a 24-bit sector address (A23-A0). The Sector Erase
instruction sequence is shown in Figure 31a.

The /CS pin must be driven high after the eighth bit of the last byte has been latched. If this is not done the
Sector Erase instruction will not be executed. After /CS is driven high, the self-timed Sector Erase
instruction will commence for a time duration of tSE (See AC Characteristics). While the Sector Erase
cycle is in progress, the Read Status Register instruction may still be accessed for checking the status of
the BUSY bit. The BUSY bit is a 1 during the Sector Erase cycle and becomes a 0 when the cycle is
finished and the device is ready to accept other instructions again. After the Sector Erase cycle has
finished the Write Enable Latch (WEL) bit in the Status Register is cleared to 0. The Sector Erase
instruction will not be executed if the addressed page is protected by the Block Protect (CMP, SEC, TB,
BP2, BP1, and BP0) bits or the Individual Block/Sector Locks.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (20h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24-Bit Address
```
```
23 22 2 1 0
```
*

```
Mode 0
```
```
Mode 3
```
*= MSB

```
Figure 31a. Sector Erase Instruction
```

##### - 39 -

#### 8.2.16 32KB Block Erase (52h)

The Block Erase instruction sets all memory within a specified block (32K-bytes) to the erased state of all
1s (FFh). A Write Enable instruction must be executed before the device will accept the Block Erase
Instruction (Status Register bit WEL must equal 1). The instruction is initiated by driving the /CS pin low
and shifting the instruction code “52h” followed a 24-bit block address (A23-A0). The Block Erase
instruction sequence is shown in Figure 32a.

The /CS pin must be driven high after the eighth bit of the last byte has been latched. If this is not done the
Block Erase instruction will not be executed. After /CS is driven high, the self-timed Block Erase instruction
will commence for a time duration of tBE 1 (See AC Characteristics). While the Block Erase cycle is in
progress, the Read Status Register instruction may still be accessed for checking the status of the BUSY
bit. The BUSY bit is a 1 during the Block Erase cycle and becomes a 0 when the cycle is finished and the
device is ready to accept other instructions again. After the Block Erase cycle has finished the Write
Enable Latch (WEL) bit in the Status Register is cleared to 0. The Block Erase instruction will not be
executed if the addressed page is protected by the Block Protect (CMP, SEC, TB, BP2, BP1, and BP0)
bits or the Individual Block/Sector Locks.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (52h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24-Bit Address
```
```
23 22 2 1 0
```
*

```
Mode 0
```
```
Mode 3
```
*= MSB

```
Figure 32a. 32KB Block Erase Instruction
```

```
Publication Release Date: December 24, 2024
```
- 40 - Revision M

#### 8.2.17 64KB Block Erase (D8h)

The Block Erase instruction sets all memory within a specified block (64K-bytes) to the erased state of all
1s (FFh). A Write Enable instruction must be executed before the device will accept the Block Erase
Instruction (Status Register bit WEL must equal 1). The instruction is initiated by driving the /CS pin low
and shifting the instruction code “D8h” followed a 24-bit block address (A23-A0). The Block Erase
instruction sequence is shown in Figure 33a.

The /CS pin must be driven high after the eighth bit of the last byte has been latched. If this is not done the
Block Erase instruction will not be executed. After /CS is driven high, the self-timed Block Erase instruction
will commence for a time duration of tBE (See AC Characteristics). While the Block Erase cycle is in
progress, the Read Status Register instruction may still be accessed for checking the status of the BUSY
bit. The BUSY bit is a 1 during the Block Erase cycle and becomes a 0 when the cycle is finished and the
device is ready to accept other instructions again. After the Block Erase cycle has finished the Write
Enable Latch (WEL) bit in the Status Register is cleared to 0. The Block Erase instruction will not be
executed if the addressed page is protected by the Block Protect (CMP, SEC, TB, BP2, BP1, and BP0)
bits or the Individual Block/Sector Locks.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (D8h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24-Bit Address
```
```
23 22 2 1 0
```
*

```
Mode 0
```
```
Mode 3
```
```
= MSB
*
```
```
Figure 33a. 64KB Block Era se Instruction
```

##### - 41 -

#### 8.2.18 Chip Erase (C7h / 60h)

The Chip Erase instruction sets all memory within the device to the erased state of all 1s (FFh). A Write
Enable instruction must be executed before the device will accept the Chip Erase Instruction (Status
Register bit WEL must equal 1). The instruction is initiated by driving the /CS pin low and shifting the
instruction code “C7h” or “60h”. The Chip Erase instruction sequence is shown in Figure 3 4.

The /CS pin must be driven high after the eighth bit has been latched. If this is not done the Chip Erase
instruction will not be executed. After /CS is driven high, the self-timed Chip Erase instruction will
commence for a time duration of tCE (See AC Characteristics). While the Chip Erase cycle is in progress,
the Read Status Register instruction may still be accessed to check the status of the BUSY bit. The BUSY
bit is a 1 during the Chip Erase cycle and becomes a 0 when finished and the device is ready to accept
other instructions again. After the Chip Erase cycle has finished the Write Enable Latch (WEL) bit in the
Status Register is cleared to 0. The Chip Erase instruction will not be executed if any memory region is
protected by the Block Protect (CMP, SEC, TB, BP2, BP1, and BP0) bits or the Individual Block/Sector
Locks.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (C7h/60h)
```
```
High Impedance
```
```
Mode 0
```
```
Mode 3
```
```
Figure 34. Chip Erase Instruction Sequence Diagram
```

```
Publication Release Date: December 24, 2024
```
- 42 - Revision M

#### 8.2.19 Erase / Program Suspend (75h)

The Erase/Program Suspend instruction “75h”, allows the system to interrupt a Sector or Block Erase
operation or a Page Program operation and then read from or program/erase data to, any other sectors or
blocks. The Erase/Program Suspend instruction sequence is shown in Figure 35a.

The Write Status Register instruction (01h) and Erase instructions (20h, 52h, D8h, C7h, 60h, 44h) are not
allowed during Erase Suspend. Erase Suspend is valid only during the Sector or Block erase operation. If
written during the Chip Erase operation, the Erase Suspend instruction is ignored. The Write Status
Register instruction (01h) and Program instructions (02h, 32h, 42h) are not allowed during Program
Suspend. Program Suspend is valid only during the Page Program or Quad Page Program operation.

The Erase/Program Suspend instruction “75h” will be accepted by the device only if the SUS bit in the
Status Register equals to 0 and the BUSY bit equals to 1 while a Sector or Block Erase or a Page
Program operation is on-going. If the SUS bit equals to 1 or the BUSY bit equals to 0, the Suspend
instruction will be ignored by the device. A maximum of time of “tSUS” (See AC Characteristics) is required
to suspend the erase or program operation. The BUSY bit in the Status Register will be cleared from 1 to
0 within “tSUS” and the SUS bit in the Status Register will be set from 0 to 1 immediately after
Erase/Program Suspend. For a previously resumed Erase/Program operation, it is also required that the
Suspend instruction “75h” is not issued earlier than a minimum of time of “tSUS” following the preceding
Resume instruction “7Ah”.

Unexpected power off during the Erase/Program suspend state will reset the device and release the
suspend state. SUS bit in the Status Register will also reset to 0. The data within the page, sector or block
that was being suspended may become corrupted. It is recommended for the user to implement system
design techniques against the accidental power interruption and preserve data integrity during
erase/program suspend state.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (75h)
```
```
High Impedance
```
```
Mode 0
```
```
Mode 3
```
```
tSUS
```
```
Accept instructions
```
```
Figure 35a. Erase/Program Suspend Instruction
```

##### - 43 -

#### 8.2.20 Erase / Program Resume (7Ah)

The Erase/Program Resume instruction “7Ah” must be written to resume the Sector or Block Erase
operation or the Page Program operation after an Erase/Program Suspend. The Resume instruction “7Ah”
will be accepted by the device only if the SUS bit in the Status Register equals to 1 and the BUSY bit
equals to 0. After issued the SUS bit will be cleared from 1 to 0 immediately, the BUSY bit will be set from
0 to 1 within 200ns and the Sector or Block will complete the erase operation or the page will complete the
program operation. If the SUS bit equals to 0 or the BUSY bit equals to 1, the Resume instruction “7Ah”
will be ignored by the device. The Erase/Program Resume instruction sequence is shown in Figure 36a.

Resume instruction is ignored if the previous Erase/Program Suspend operation was interrupted by
unexpected power off. It is also required that a subsequent Erase/Program Suspend instruction not to be
issued within a minimum of time of “tSUS” following a previous Resume instruction.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (7Ah)
```
```
Mode 0
```
```
Mode 3
```
```
Resume previously
suspended Program or
Erase
```
```
Figure 36a. Erase/Program Resume Instruction
```

```
Publication Release Date: December 24, 2024
```
- 44 - Revision M

#### 8.2.21 Power-down (B9h)

Although the standby current during normal operation is relatively low, standby current can be further
reduced with the Power-down instruction. The lower power consumption makes the Power-down
instruction especially useful for battery powered applications (See ICC1 and ICC2 in AC Characteristics).
The instruction is initiated by driving the /CS pin low and shifting the instruction code “B9h” as shown in
Figure 37a.

The /CS pin must be driven high after the eighth bit has been latched. If this is not done the Power-down
instruction will not be executed. After /CS is driven high, the power-down state will entered within the time
duration of tDP (See AC Characteristics). While in the power-down state only the Release Power-down /
Device ID (ABh) instruction, which restores the device to normal operation, will be recognized. All other
instructions are ignored. This includes the Read Status Register instruction, which is always available
during normal operation. Ignoring all but one instruction makes the Power Down state a useful condition
for securing maximum write protection. The device always powers-up in the normal operation with the
standby current of ICC1.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (B9h)
```
```
Mode 0
```
```
Mode 3
```
```
tDP
```
```
Stand-by current Power-down current
```
```
Figure 37a. Deep Power-down Instruction
```

##### - 45 -

#### 8.2.22 Release Power-down / Device ID (ABh)

The Release from Power-down / Device ID instruction is a multi-purpose instruction. It can be used to
release the device from the power-down state, or obtain the devices electronic identification (ID) number.

To release the device from the power-down state, the instruction is issued by driving the /CS pin low,
shifting the instruction code “ABh” and driving /CS high as shown in Figure 38a. Release from power-down
will take the time duration of tRES 1 (See AC Characteristics) before the device will resume normal
operation and other instructions are accepted. The /CS pin must remain high during the tRES 1 time
duration.

When used only to obtain the Device ID while not in the power-down state, the instruction is initiated by
driving the /CS pin low and shifting the instruction code “ABh” followed by 3-dummy bytes. The Device ID
bits are then shifted out on the falling edge of CLK with most significant bit (MSB) first. The Device ID
value for the W25Q128JV is listed in Manufacturer and Device Identification table. The Device ID can be
read continuously. The instruction is completed by driving /CS high.

When used to release the device from the power-down state and obtain the Device ID, the instruction is
the same as previously described, and shown in Figure 38c, except that after /CS is driven high it must
remain high for a time duration of tRES 2 (See AC Characteristics). After this time duration the device will
resume normal operation and other instructions will be accepted. If the Release from Power-down /
Device ID instruction is issued while an Erase, Program or Write cycle is in process (when BUSY equals
1) the instruction is ignored and will not have any effects on the current cycle.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (ABh)
```
```
Mode 0
```
```
Mode 3
```
```
tRES1
```
```
Power-down current Stand-by current
```
```
Figure 38a. Release Power-down Instruction
```
```
tRES2
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (ABh)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
3 Dummy Bytes
23 22 2 1 0
```
#### *

```
Mode 0
```
```
Mode 3
```
```
7 6 5 4 3 2 1 0
```
#### *

```
32 33 34 35 36 37 38
```
```
Device ID
```
#### *= MSB Power-down current Stand-by current

```
Figure 38c. Release Power-down / Device ID Instruction
```

```
Publication Release Date: December 24, 2024
```
- 46 - Revision M

#### 8.2.23 Read Manufacturer / Device ID (90h)

The Read Manufacturer/Device ID instruction is an alternative to the Release from Power-down / Device
ID instruction that provides both the JEDEC assigned manufacturer ID and the specific device ID.

The Read Manufacturer/Device ID instruction is very similar to the Release from Power-down / Device ID
instruction. The instruction is initiated by driving the /CS pin low and shifting the instruction code “90h”
followed by a 24-bit address (A23-A0) of 000000h. After which, the Manufacturer ID for Winbond (EFh)
and the Device ID are shifted out on the falling edge of CLK with most significant bit (MSB) first as shown
in Figure 39. The Device ID values for the W25Q128JV are listed in Manufacturer and Device
Identification table. The instruction is completed by driving /CS high.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (90h)
```
```
High Impedance
```
```
8 9 10 28 29 30 31
```
```
Address (000000h)
```
```
23 22 21 3 2 1 0
```
```
Device ID
```
*

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
32 33 34 35 36 37 38 39
```
```
Manufacturer ID (EFh)
```
```
40 41 42 44 45 46
```
```
7 6 5 4 3 2 1 0
```
*

```
31 43
```
```
0
```
```
Mode 0
```
```
Mode 3
```
*= MSB

```
Figure 39. Read Manufacturer / Device ID Instruction
```

##### - 47 -

#### 8.2.24 Read Manufacturer / Device ID Dual I/O (92h)

The Read Manufacturer / Device ID Dual I/O instruction is an alternative to the Read Manufacturer /
Device ID instruction that provides both the JEDEC assigned manufacturer ID and the specific device ID
at 2x speed.

The Read Manufacturer / Device ID Dual I/O instruction is similar to the Fast Read Dual I/O instruction.
The instruction is initiated by driving the /CS pin low and shifting the instruction code “92h” followed by a
24 - bit address (A23-A0) of 000000h, but with the capability to input the Address bits two bits per clock.
After which, the Manufacturer ID for Winbond (EFh) and the Device ID are shifted out 2 bits per clock on
the falling edge of CLK with most significant bits (MSB) first as shown in Figure 40. The Device ID values
for the W25Q128JV are listed in Manufacturer and Device Identification table. The Manufacturer and
Device IDs can be read continuously, alternating from one to the other. The instruction is completed by
driving /CS high.^

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (92h)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15 16 17 18 19 20 21 22
```
```
7 5 3 1
```
## * *

```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
23
```
## * *

```
A23-16 A15-8 A7-0 (00h) M7-0
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38
```
```
0
```
```
Mode 0
```
```
Mode 3
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3 1
```
```
6 4 2 0
```
```
7 5 3
```
```
6 4 2
```
```
1
```
```
0
```
```
1
MFR ID Device ID MFR ID(repeat) Device ID(repeat)
```
```
IOs switch from
Input to Output
```
## * * * *

## *= MSB

```
Figure 40. Read Manufacturer / Device ID Dual I/O Instruction
```
```
Note:
The “Continuous Read Mode” bits M(7-0) must be set to Fxh to be compatible with Fast Read Dual I/O instruction.
```

```
Publication Release Date: December 24, 2024
```
- 48 - Revision M

#### 8.2.25 Read Manufacturer / Device ID Quad I/O (94h)

The Read Manufacturer / Device ID Quad I/O instruction is an alternative to the Read Manufacturer /
Device ID instruction that provides both the JEDEC assigned manufacturer ID and the specific device ID
at 4x speed.

The Read Manufacturer / Device ID Quad I/O instruction is similar to the Fast Read Quad I/O instruction.
The instruction is initiated by driving the /CS pin low and shifting the instruction code “94h” followed by a
four clock dummy cycles and then a 24-bit address (A23-A0) of 000000h, but with the capability to input
the Address bits four bits per clock. After which, the Manufacturer ID for Winbond (EFh) and the Device ID
are shifted out four bits per clock on the falling edge of CLK with most significant bit (MSB) first as shown
in Figure 41. The Device ID values for the W25Q128JV are listed in Manufacturer and Device
Identification table. The Manufacturer and Device IDs can be read continuously, alternating from one to
the other. The instruction is completed by driving /CS high.^

```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (94h)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15 16 17 18 19 20 21 22
```
```
5 1
```
```
4 0
```
```
23
```
```
Mode 0
```
```
Mode 3
```
```
IOs switch from
Input to Output
```
```
High Impedance
7 3
```
```
6 2
```
```
/CS
```
```
CLK
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
High Impedance
```
```
A23-16 A15-8 (00h)A7-0 M7-0
```
```
MFR ID Device ID
```
```
Dummy Dummy
```
```
/CS
```
```
CLK
```
```
IO 0
```
```
IO 1
```
```
IO 2
```
```
IO 3
```
```
23
```
```
0
```
```
1
```
```
2
```
```
3
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
5 1
```
```
4 0
```
```
7 3
```
```
6 2
```
```
24 25 26 27 28 29 30
```
```
MFR ID
(repeat)
```
```
Device ID
(repeat)
```
```
MFR ID
(repeat)
```
```
Device ID
(repeat)
```
```
Figure 41. Read Manufacturer / Device ID Quad I/O Instruction
```
```
Note:
The “Continuous Read Mode” bits M(7-0) must be set to Fxh to be compatible with Fast Read Quad I/O instruction.
```

##### - 49 -

#### 8.2.26 Read Unique ID Number (4Bh) - 3 -

The Read Unique ID Number instruction accesses a factory-set read-only 64-bit number that is unique to
each W25Q128JV device. The ID number can be used in conjunction with user software methods to help
prevent copying or cloning of a system. The Read Unique ID instruction is initiated by driving the /CS pin
low and shifting the instruction code “4Bh” followed by a four bytes of dummy clocks. After which, the 64-
bit ID is shifted out on the falling edge of CLK as shown in Figure 42.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (4Bh)
```
```
High Impedance
```
```
8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38
Mode 0
```
```
Mode 3
```
## *

```
Dummy Byte 1 Dummy Byte 2
```
```
39 40 41 42
```
```
Dummy Byte 3 Dummy Byte 4
```
```
63 62 61 2 1 0
64-bit Unique Serial Number
```
```
100101102
```
```
High Impedance
```
## *= MSB

```
Figure 42. Read Unique ID Number Instruction
```

```
Publication Release Date: December 24, 2024
```
- 50 - Revision M

#### 8.2.27 Read JEDEC ID (9Fh)

For compatibility reasons, the W25Q128JV provides several instructions to electronically determine the
identity of the device. The Read JEDEC ID instruction is compatible with the JEDEC standard for SPI
compatible serial memories that was adopted in 2003. The instruction is initiated by driving the /CS pin low
and shifting the instruction code “9Fh”. The JEDEC assigned Manufacturer ID byte for Winbond (EFh) and
two Device ID bytes, Memory Type (ID15-ID8) and Capacity (ID7-ID0) are then shifted out on the falling
edge of CLK with most significant bit (MSB) first as shown in Figure 43a. For memory type and capacity
values refer to Manufacturer and Device Identification table.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (9Fh)
```
```
High Impedance
```
```
8 9 10 12 13 14 15
```
```
Capacity ID7-0
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
16 17 18 19 20 21 22 23
```
```
Manufacturer ID (EFh)
```
```
24 25 26 28 29 30
```
```
7 6 5 4 3 2 1 0
*
```
```
15 27
Mode 0
```
```
Mode 3
```
```
11
```
```
7 6 5 4 3 2 1 0
*
```
```
Memory Type ID15-8
```
*= MSB

```
Figure 43a. Read JEDEC ID Instruction
```

##### - 51 -

#### 8.2.28 Read SFDP Register (5Ah)

The W25Q128JV features a 256-Byte Serial Flash Discoverable Parameter (SFDP) register that contains
information about device configurations, available instructions and other features. The SFDP parameters
are stored in one or more Parameter Identification (PID) tables. Currently only one PID table is specified,
but more may be added in the future. The Read SFDP Register instruction is compatible with the SFDP
standard initially established in 2010 for PC and other applications, as well as the JEDEC standard
JESD216-serials that is published in 2011. Most Winbond SpiFlash Memories shipped after June 2011
(date code 1124 and beyond) support the SFDP feature as specified in the applicable datasheet.

The Read SFDP instruction is initiated by driving the /CS pin low and shifting the instruction code “5Ah”
followed by a 24-bit address (A23-A0)(1) into the DI pin. Eight “dummy” clocks are also required before the
SFDP register contents are shifted out on the falling edge of the 40th CLK with most significant bit (MSB)
first as shown in Figure 44. For SFDP register values and descriptions, please refer to the Winbond
Application Note for SFDP Definition Table.

Note 1: A23-A8 = 0; A7-A0 are used to define the starting byte address for the 256-Byte SFDP Register.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (5Ah)
```
```
High Impedance
```
```
8 9 10 28 29 30 31
```
```
24-Bit Address
23 22 21 3 2 1 0
```
```
Data Out 1
```
## *

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
32 33 34 35 36 37 38 39
```
```
Dummy Byte
```
```
High Impedance
```
```
40 41 42 44 45 46 47 48 49 50 51 52 53 54 55
```
```
7 6 5 4 3 2 1 0 7
```
```
Data Out 2
```
## *

```
7 6 5 4 3 2 1 0
```
## *

```
7 6 5 4 3 2 1 0
```
```
31 43
```
```
0
```
## *= MSB

```
Figure 44. Read SFDP Register Instruction Sequence Diagram
```

```
Publication Release Date: December 24, 2024
```
- 52 - Revision M

#### 8.2.29 Erase Security Registers (44h)

The W25Q128JV offers three 256-byte Security Registers which can be erased and programmed
individually. These registers may be used by the system manufacturers to store security and other
important information separately from the main memory array.

The Erase Security Register instruction is similar to the Sector Erase instruction. A Write Enable
instruction must be executed before the device will accept the Erase Security Register Instruction (Status
Register bit WEL must equal 1). The instruction is initiated by driving the /CS pin low and shifting the
instruction code “44h” followed by a 24-bit address (A23-A0) to erase one of the three security registers.

```
ADDRESS A23- 16 A15- 12 A11- 8 A7- 0
```
```
Security Register #1 00h 0 0 0 1 0 0 0 0 Don’t Care
Security Register #2 00h 0 0 1 0 0 0 0 0 Don’t Care
Security Register #3 00h 0 0 1 1 0 0 0 0 Don’t Care
```
The Erase Security Register instruction sequence is shown in Figure 45. The /CS pin must be driven high
after the eighth bit of the last byte has been latched. If this is not done the instruction will not be executed.
After /CS is driven high, the self-timed Erase Security Register operation will commence for a time
duration of tSE (See AC Characteristics). While the Erase Security Register cycle is in progress, the Read
Status Register instruction may still be accessed for checking the status of the BUSY bit. The BUSY bit is
a 1 during the erase cycle and becomes a 0 when the cycle is finished and the device is ready to accept
other instructions again. After the Erase Security Register cycle has finished the Write Enable Latch
(WEL) bit in the Status Register is cleared to 0. The Security Register Lock Bits (LB3-1) in the Status
Register-2 can be used to OTP protect the security registers. Once a lock bit is set to 1, the corresponding
security register will be permanently locked, Erase Security Register instruction to that register will be
ignored (Refer to section 7.1. 9 for detail descriptions).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (44h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24-Bit Address
```
```
23 22 2 1 0
```
*

```
Mode 0
```
```
Mode 3
```
```
= MSB
*
```
```
Figure 45. Erase Security Registers Instruction
```

##### - 53 -

#### 8.2.30 Program Security Registers (42h)

The Program Security Register instruction is similar to the Page Program instruction. It allows from one
byte to 256 bytes of security register data to be programmed at previously erased (FFh) memory locations.
A Write Enable instruction must be executed before the device will accept the Program Security Register
Instruction (Status Register bit WEL= 1). The instruction is initiated by driving the /CS pin low then shifting
the instruction code “42h” followed by a 24-bit address (A23-A0) and at least one data byte, into the DI pin.
The /CS pin must be held low for the entire length of the instruction while data is being sent to the device.

```
ADDRESS A23- 16 A15- 12 A11- 8 A7- 0
```
```
Security Register #1 00h 0 0 0 1 0 0 0 0 Byte Address
Security Register #2 00h 0 0 1 0 0 0 0 0 Byte Address
Security Register #3 00h 0 0 1 1 0 0 0 0 Byte Address
```
The Program Security Register instruction sequence is shown in Figure 46. The Security Register Lock
Bits (LB3-1) in the Status Register-2 can be used to OTP protect the security registers. Once a lock bit is
set to 1, the corresponding security register will be permanently locked, Program Security Register
instruction to that register will be ignored (See 7.1. 9 for detail descriptions).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (42h)
```
```
8 9 10 28 29 30 39
```
```
24-Bit Address
23 22 21 3 2 1
```
## *

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
40 41 42 43 44 45 46 47
```
```
Data Byte 2
```
(^484950525354552072)
7 6 5 4 3 2 1 0
39 51
0
31
0
32 33 34 35 36 37 38
Data Byte 1
7 6 5 4 3 2 1

## *

```
Mode 0
```
```
Mode 3
```
```
Data Byte 3
```
```
2073207420752076207720782079
```
```
0
```
```
Data Byte 256
```
## *

```
7 6 5 4 3 2 1 0
```
## *

```
7 6 5 4 3 2 1 0
```
## *

## *= MSB

```
Figure 46. Program Security Registers Instruction
```

```
Publication Release Date: December 24, 2024
```
- 54 - Revision M

#### 8.2.31 Read Security Registers (48h)

The Read Security Register instruction is similar to the Fast Read instruction and allows one or more data
bytes to be sequentially read from one of the four security registers. The instruction is initiated by driving
the /CS pin low and then shifting the instruction code “48h” followed by a 24-bit address (A23-A0) and
eight “dummy” clocks into the DI pin. The code and address bits are latched on the rising edge of the CLK
pin. After the address is received, the data byte of the addressed memory location will be shifted out on
the DO pin at the falling edge of CLK with most significant bit (MSB) first. The byte address is
automatically incremented to the next byte address after each byte of data is shifted out. Once the byte
address reaches the last byte of the register (byte address FFh), it will reset to address 00h, the first byte
of the register, and continue to increment. The instruction is completed by driving /CS high. The Read
Security Register instruction sequence is shown in Figure 47. If a Read Security Register instruction is
issued while an Erase, Program or Write cycle is in process (BUSY=1) the instruction is ignored and will
not have any effects on the current cycle. The Read Security Register instruction allows clock rates from
D.C. to a maximum of FR (see AC Electrical Characteristics).

```
ADDRESS A23- 16 A15- 12 A11- 8 A7- 0
```
```
Security Register #1 00h 0 0 0 1 0 0 0 0 Byte Address
Security Register #2 00h 0 0 1 0 0 0 0 0 Byte Address
Security Register #3 00h 0 0 1 1 0 0 0 0 Byte Address
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (48h)
```
```
High Impedance
```
```
8 9 10 28 29 30 31
```
```
24-Bit Address
23 22 21 3 2 1 0
```
```
Data Out 1
```
## *

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
32 33 34 35 36 37 38 39
```
```
Dummy Byte
```
```
High Impedance
```
```
40 41 42 44 45 46 47 48 49 50 51 52 53 54 55
```
```
7 6 5 4 3 2 1 0 7
```
```
Data Out 2
```
## *

```
7 6 5 4 3 2 1 0
```
## *

```
7 6 5 4 3 2 1 0
```
```
31 43
```
```
0
```
## *= MSB

```
Figure 47. Read Security Registers Instruction
```

##### - 55 -

#### 8.2.32 Individual Block/Sector Lock (36h)

The Individual Block/Sector Lock provides an alternative way to protect the memory array from adverse
Erase/Program. In order to use the Individual Block/Sector Locks, the WPS bit in Status Register-3 must
be set to 1. If WPS=0, the write protection will be determined by the combination of CMP, SEC, TB,
BP[2:0] bits in the Status Registers. The Individual Block/Sector Lock bits are volatile bits. The default
values after device power up or after a Reset are 1, so the entire memory array is being protected.

To lock a specific block or sector as illustrated in Figure 4d, an Individual Block/Sector Lock command
must be issued by driving /CS low, shifting the instruction code “36h” into the Data Input (DI) pin on the
rising edge of CLK, followed by a 24-bit address and then driving /CS high. A Write Enable instruction
must be executed before the device will accept the Individual Block/Sector Lock Instruction (Status
Register bit WEL= 1).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction ( 36 h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24 - Bit Address
23 22 2 1 0
```
*

```
Mode 0
```
```
Mode 3
```
*=^ MSB

```
Figure 5 3 a. Individual Block/Sector Lock Instruction
```

```
Publication Release Date: December 24, 2024
```
- 56 - Revision M

#### 8.2.33 Individual Block/Sector Unlock (39h)..................................................................................

The Individual Block/Sector Lock provides an alternative way to protect the memory array from adverse
Erase/Program. In order to use the Individual Block/Sector Locks, the WPS bit in Status Register-3 must
be set to 1. If WPS=0, the write protection will be determined by the combination of CMP, SEC, TB,
BP[2:0] bits in the Status Registers. The Individual Block/Sector Lock bits are volatile bits. The default
values after device power up or after a Reset are 1, so the entire memory array is being protected.

To unlock a specific block or sector as illustrated in Figure 4d, an Individual Block/Sector Unlock
command must be issued by driving /CS low, shifting the instruction code “39h” into the Data Input (DI) pin
on the rising edge of CLK, followed by a 24-bit address and then driving /CS high. A Write Enable
instruction must be executed before the device will accept the Individual Block/Sector Unlock Instruction
(Status Register bit WEL= 1).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction ( 39 h)
```
```
High Impedance
```
```
8 9 29 30 31
```
```
24 - Bit Address
23 22 2 1 0
```
## *

```
Mode 0
```
```
Mode 3
```
## *=^ MSB

```
Figure 5 4 a. Individual Block Unlock Instruction
```

##### - 57 -

#### 8.2.34 Read Block/Sector Lock (3Dh)

The Individual Block/Sector Lock provides an alternative way to protect the memory array from adverse
Erase/Program. In order to use the Individual Block/Sector Locks, the WPS bit in Status Register-3 must
be set to 1. If WPS=0, the write protection will be determined by the combination of CMP, SEC, TB,
BP[2:0] bits in the Status Registers. The Individual Block/Sector Lock bits are volatile bits. The default
values after device power up or after a Reset are 1, so the entire memory array is being protected.

To read out the lock bit value of a specific block or sector as illustrated in Figure 4d, a Read Block/Sector
Lock command must be issued by driving /CS low, shifting the instruction code “3Dh” into the Data Input
(DI) pin on the rising edge of CLK, followed by a 24-bit address. The Block/Sector Lock bit value will be
shifted out on the DO pin at the falling edge of CLK with most significant bit (MSB) first as shown in Figure
55. If the least significant bit (LSB) is 1, the corresponding block/sector is locked; if LSB=0, the
corresponding block/sector is unlocked, Erase/Program operation can be performed.

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction ( 3 Dh)
```
```
High Impedance
```
```
8 9 10 28 29 30 31 32 33 34 35 36 37 38 39
```
```
X X X X X X X 0
```
```
24 - Bit Address
23 22 21 3 2 1 0
```
```
Lock Value Out
```
#### *

#### *

#### *=^ MSB

```
Mode 0
```
```
Mode 3
```
```
Figure 5 5 a. Read Block Lock Instruction
```

```
Publication Release Date: December 24, 2024
```
- 58 - Revision M

#### 8.2.35 Global Block/Sector Lock (7Eh)

All Block/Sector Lock bits can be set to 1 by the Global Block/Sector Lock instruction. The command must
be issued by driving /CS low, shifting the instruction code “7Eh” into the Data Input (DI) pin on the rising
edge of CLK, and then driving /CS high. A Write Enable instruction must be executed before the device
will accept the Global Block/Sector Lock Instruction (Status Register bit WEL= 1).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
Mode 0
```
```
Mode 3
```
```
Instruction ( 7 Eh)
```
```
High Impedance
```
```
Figure 5 6. Global Block Lock Instruction for SPI Mode
```
#### 8.2.36 Global Block/Sector Unlock (98h)

All Block/Sector Lock bits can be set to 0 by the Global Block/Sector Unlock instruction. The command
must be issued by driving /CS low, shifting the instruction code “98h” into the Data Input (DI) pin on the
rising edge of CLK, and then driving /CS high. A Write Enable instruction must be executed before the
device will accept the Global Block/Sector Unlock Instruction (Status Register bit WEL= 1).

```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
Mode 0
```
```
Mode 3
```
```
Instruction ( 98 h)
```
```
High Impedance
```
```
Figure 5 7. Global Block Unlock Instruction for SPI Mode
```

##### - 59 -

#### Enable Reset (66h) and Reset Device (99h)

Because of the small package and the limitation on the number of pins, the W25Q128JV provide a
software Reset instruction instead of a dedicated RESET pin. Once the Reset instruction is accepted, any
on-going internal operations will be terminated and the device will return to its default power-on state and
lose all the current volatile settings, such as Volatile Status Register bits, Write Enable Latch (WEL)
status, Program/Erase Suspend status, Read parameter setting (P7-P0), and Wrap Bit setting (W6-W4).

“Enable Reset (66h)” and “Reset (99h)” instructions can be issued in SPI. To avoid accidental reset, both
instructions must be issued in sequence. Any other commands other than “Reset (99h)” after the “Enable
Reset (66h)” command will disable the “Reset Enable” state. A new sequence of “Enable Reset (66h)” and
“Reset (99h)” is needed to reset the device. Once the Reset command is accepted by the device, the
device will take approximately tRST=30us to reset. During this period, no command will be accepted.

Data corruption may happen if there is an on-going or suspended internal Erase or Program operation
when Reset command sequence is accepted by the device. It is recommended to check the BUSY bit and
the SUS bit in Status Register before issuing the Reset command sequence.

```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (99h)
```
```
Mode 0
```
```
Mode 3
```
```
/CS
```
```
CLK
```
```
DI
(IO 0 )
```
```
DO
(IO 1 )
```
```
Mode 0
```
```
Mode 3 0 1 2 3 4 5 6 7
```
```
Instruction (66h)
```
```
High Impedance
```
```
Figure 58 a. Enable Reset and Reset Instruction Sequence
```

```
Publication Release Date: December 24, 2024
```
- 60 - Revision M

## 9. ELECTRICAL CHARACTERISTICS

### 9.1 Absolute Maximum Ratings (1)

```
PARAMETERS SYMBOL CONDITIONS RANGE UNIT
```
```
Supply Voltage VCC – 0.6 to 4.6 V
Voltage Applied to Any Pin VIO Relative to Ground – 0.6 to VCC+0.4 V
```
```
Transient Voltage on any Pin VIOT
```
```
<20nS Transient
Relative to Ground –^2 .0V to VCC+^2 .0V^ V^
```
```
Storage Temperature TSTG – 65 to +150 °C
```
```
Lead Temperature TLEAD See Note (^2 ) °C
```
```
Electrostatic Discharge Voltage^ VESD Human Body Model(^3 )^ – 2000 to +2000 V
```
Notes:

1. This device has been designed and tested for the specified operation ranges. Proper operation outside
of these levels is not guaranteed. Exposure to absolute maximum ratings may affect device reliability.
Exposure beyond absolute maximum ratings may cause permanent damage.

2. Compliant with JEDEC Standard J-STD-20C for small body Sn-Pb or Pb-free (Green) assembly and the
European directive on restrictions on hazardous substances (RoHS) 2002/95/EU.

3. JEDEC Std JESD22-A114A (C1=100pF, R1=1500 ohms, R2=500 ohms).

### 9.2 Operating Ranges...............................................................................................................

```
PARAMETER SYMBOL CONDITIONS
```
```
SPEC
UNIT
MIN MAX
```
```
Supply Voltage(1) VCC
```
```
FR = 133MHz, fR = 50 MHz 3.0 3.6 V
```
```
FR = 104MHz, fR = 50 MHz 2.7 3. 6 V
```
```
Ambient Temperature,
Operating TA^
```
```
Industrial – 40 +85 °C
```
```
Industrial Plus – 40 +10 5 °C
```
Note:

1. VCC voltage during Read can operate across the min and max range but should not exceed ±10% of
    the programming (erase/write) voltage.


##### - 61 -

### 9.3 Power-Up Power-Down Timing and Requirements

```
PARAMETER SYMBOL
```
```
SPEC
UNIT
MIN MAX
VCC (min) to /CS Low tVSL(1) 20 μs
```
```
Time Delay Before Write Instruction tPUW(1) 5 ms
```
```
Write Inhibit Threshold Voltage VWI(1) 1.0 2.0 V
The minimum duration for ensuring
initialization will occur
```
```
tPWD(1) 100 μs
```
```
VCC voltage needed to below VPWD for
ensuring initialization will occur
```
##### VPWD(1) 0.8 V

Note:

1. These parameters are characterized only.

```
VCC
```
```
tVSL Read Instructions
Allowed
```
```
Device is fully
Accessible
```
```
tPUW
```
```
/CS must track VCC
```
```
Program, Erase and Write Instructions are ignored
```
```
Reset
State
```
```
VCC (max)
```
```
VCC (min)
```
```
VWI
```
```
Time
```
```
Figure 58a. Power-up Timing and Voltage Levels
```

```
Publication Release Date: December 24, 2024
```
- 62 - Revision M

```
V C C
```
```
T im e
```
```
/C S m u s t tra c k V C C
d u rin g V C C R a m p U p/D o w n
```
```
/CS
```
```
Figure 58b. Power-up, Power-Down Requirement
```
#### 9.3.1 Power Cycle Requirement

For power cycle, the system must not initial the power-up sequence until Vcc drops down to VPWD and
keeps a tPWD for device to initialize correctly.

```
Figure 58c. Power Cycle Requirement
```
##### VCC

t (^) VSL Device is fully accessible^
VCC (max )
VCC (^) ( min )
Time
V (^) PWD (max)
t (^) PWD
Chip select is not acceptable


##### - 63 -

### 9.4 DC Electrical Characteristics-

```
PARAMETER SYMBOL CONDITIONS
```
```
SPEC
UNIT
MIN TYP MAX
Input Capacitance CIN(1) VIN = 0V(1)^6 pF
```
Output Capacitance Cout(1) VOUT = 0V(1) (^8) pF
Input Leakage ILI ±2 μA
I/O Leakage ILO (^) ±2 μA
Standby Current ICC 1 /CS = VCC, VIN = GND or VCC^10 60 μA
Power-down Current ICC 2 /CS = VCC, VIN = GND or VCC^1 20 μA
Current Read Data /
Dual /Quad 50MHz(2) ICC^3
C = 0.1 VCC / 0.9 VCC
DO = Open 8 15 mA^
Current Read Data /
Dual /Quad 80MHz(2) ICC^3
C = 0.1 VCC / 0.9 VCC
DO = Open 10 18 mA^
Current Read Data /
Dual /Quad 104MHz(2) ICC^3
C = 0.1 VCC / 0.9 VCC
DO = Open^12 20 mA^
Current Write Status
Register ICC^4 /CS = VCC^20 25 mA^
Current Page Program ICC 5 /CS = VCC 20 25 mA
Current Sector/Block
Erase ICC^6 /CS = VCC^20 25 mA^
Current Chip Erase ICC 7 /CS = VCC 20 25 mA
Input Low Voltage VIL (^) – 0.5 VCC x 0.3 V
Input High Voltage VIH VCC x 0.7 VCC + 0.4 V
Output Low Voltage VOL IOL = 100 μA (^) 0. 2 V
Output High Voltage VOH IOH = –100 μA VCC – 0.2 V
Notes:
1. Tested on sample basis and specified through design and characterization data. TA = 25° C, VCC = 3.0V.
2. Checker Board Pattern.


```
Publication Release Date: December 24, 2024
```
- 64 - Revision M

### 9.5 AC Measurement Conditions

```
PARAMETER SYMBOL
```
```
SPEC
UNIT
MIN MAX
Load Capacitance CL 30 pF
```
```
Input Rise and Fall Times TR, TF 5 ns
```
```
Input Pulse Voltages VIN 0. 1 VCC to 0. 9 VCC V
Input Timing Reference Voltages IN 0.3 VCC to 0.7 VCC V
```
```
Output Timing Reference Voltages OUT 0. 5 VCC to 0. 5 VCC V
```
Note:

1. Output Hi-Z is defined as the point where data out is no longer driven.

```
Input and Output
Input Levels Timing Reference Levels
```
```
0.9 VCC
```
```
0.1 VCC
```
```
0.5 VCC
```
```
Figure 59. AC Measurement I/O Waveform
```

##### - 65 -

### 9.6 AC Electrical Characteristics(6)

```
DESCRIPTION SYMBOL ALT
```
```
SPEC
UNIT
MIN TYP MAX
```
```
Clock frequency except for Read Data (03h)
instructions (3.0V-3.6V)
```
```
FR fC1 D.C. 133 MHz
```
```
Clock frequency except for Read Data (03h)
instructions( 2.7V-3.0V)
```
```
FR fC 2 D.C. 104 MHz
```
```
Clock frequency for Read Data instruction (03h) fR^ D.C.^50 MHz^
```
```
Clock High, Low Time
for all instructions except for Read Data (03h)
```
```
tCLH,^
tCLL(1)
```
(^4) 5%
PC
ns
Clock High, Low Time
for Read Data (03h) instruction
tCRLH,
tCRLL(1)
(^4) 5%
PC ns^
Clock Rise Time peak to peak tCLCH(2) 0.1 V/ns
Clock Fall Time peak to peak tCHCL(2) 0.1 V/ns
/CS Active Setup Time relative to CLK tSLCH tCSS 3 ns
/CS Not Active Hold Time relative to CLK tCHSL 3 ns
Data In Setup Time tDVCH tDSU 1 ns
Data In Hold Time tCHDX tDH 2 ns
/CS Active Hold Time relative to CLK tCHSH 3 ns
/CS Not Active Setup Time relative to CLK tSHCH 3 ns
/CS Deselect Time (for Read) tSHSL 1 tCSH 10 ns
/CS Deselect Time (for Erase or Program or Write) tSHSL 2 tCSH 50 ns
Output Disable Time tSHQZ(2) tDIS 7 ns
Clock Low to Output Valid
tCLQV^ tV^6 ns^
Output Hold Time tCLQX tHO 1.5 ns
Continued – next page AC Electrical Characteristics (cont’d)


```
Publication Release Date: December 24, 2024
```
- 66 - Revision M

```
DESCRIPTION SYMBOL ALT
```
```
SPEC
UNIT
MIN TYP MAX
```
```
Write Protect Setup Time Before /CS Low tWHSL(3) 20 ns
```
```
Write Protect Hold Time After /CS High tSHWL(3) 100 ns
```
```
/CS High to Power-down Mode tDP(2) 3 μs
```
```
/CS High to Standby Mode without ID Read tRES 1 (2) 3 μs
```
```
/CS High to Standby Mode with ID Read tRES 2 (2) 1.8 μs
```
```
/CS High to next Instruction after Suspend tSUS(2) 20 μs
```
```
/CS High to next Instruction after Reset tRST(2) 30 μs
```
```
/RESET pin Low period to reset the device tRESET(2) 1 (^4 ) μs
```
```
Write Status Register Time tW 10 15 ms
```
```
Page Program Time tPP 0.4 3 ms
```
```
Sector Erase Time (4KB) tSE 45 400 ms
```
```
Block Erase Time ( 32 KB) tBE 1 120 1 , 600 ms
```
```
Block Erase Time (64KB) tBE 2 150 2 ,000 ms
```
```
Chip Erase Time tCE 40 200 s
```
Notes:

1. Clock high or Clock low must be more than or equal to 45%Pc. Pc = 1/fc(max).
2. Value guaranteed by design and/or characterization, not 100% tested in production.
3. Only applicable as a constraint for a Write Status Register instruction when SRP=1.
4. It’s possible to reset the device with shorter tRESET (as short as a few hundred ns), a 1us minimum is recommended to
    ensure reliable operation.
5. Tested on sample basis and specified through design and characterization data. TA = 25° C, VCC = 3.0V, 25% driver
    strength.
6. 4 - bytes address alignment for Quad Read


##### - 67 -

### 9.7 Serial Output Timing

```
/CS
```
```
CLK
```
```
IO
output
```
```
tCLQX
```
```
tCLQV
tCLQX
```
```
tCLQV tCLL tSHQZ
```
```
LSB OUT
```
```
tCLH
```
```
MSB OUT
```
### 9.8 Serial Input Timing

```
/CS
```
```
CLK
```
```
IO
input
```
```
tCHSL
```
```
MSB IN
```
```
tSLCH
```
```
tDVCH tCHDX
```
```
tCHSH tSHCH
```
```
tCLCH tCHCL
LSB IN
```
```
tSHSL
```
### 9.9 /WP Timing

```
/CS
```
```
CLK
```
```
/WP
```
```
tWHSL tSHWL
```
```
IO
input
Write Status Register is allowed Write Status Register is not allowed
```

```
Publication Release Date: December 24, 2024
```
- 68 - Revision M

## 10. PACKAGE SPECIFICATIONS

### 10.1 8 - Pin SOIC 208-mil (Package Code S)

```
Symbol
```
```
Millimeters Inches
```
```
Min Nom Max Min Nom Max
A 1.75 1.95 2.16 0.069 0.077 0.085
A1 0.05 0.15 0.25 0.002 0.006 0.010
A2 1.70 1.80 1.91 0.067 0.071 0.075
b 0.35 0.42 0.48 0.014 0.017 0.019
C 0.19 0.20 0.25 0.007 0.008 0.010
D 5.18 5.28 5.38 0.204 0.208 0.212
D1 5.13 5.23 5.33 0.202 0.206 0.210
E 5.18 5.28 5.38 0.204 0.208 0.212
E1 5.13 5.23 5.33 0.202 0.206 0.210
e 1.27 BSC 0.050 BSC
H 7.70 7.90 8.10 0.303 0.311 0.319
L 0.50 0.65 0.80 0.020 0.026 0.031
y --- --- 0.10 --- --- 0.004
θ 0° --- 8° 0° --- 8°
Note: Both the package length and width do not include the mold flash. (Refer JEDEC MS-012)
```

##### - 69 -

### 10.2 16 - Pin SOIC 300-mil (Package Code F)

```
Symbol
```
```
Millimeters Inches
Min Nom Max Min Nom Max
A 2.36 2.49 2.64 0 .093 0.098 0.104
A1 0.10 --- 0.30 0.004 --- 0.012
A2 --- 2.31 --- --- 0.091 ---
b 0.33 0.41 0.51 0.013 0.016 0.020
C 0.18 0.23 0.28 0.007 0.009 0.011
D 10.08 10.31 10.49 0.397 0.406 0.413
E 10.01 10.31 10.64 0.394 0.406 0.419
E1 7.39 7.49 7.59 0.291 0.295 0.299
e 1.27 BSC 0.050 BSC
L 0.38 0.81 1.27 0.015 0.032 0.050
y --- --- 0. 10 --- --- 0.00 4
θ 0° --- 8° 0° --- 8°
```
```
Note: Both the package length and width do not include the mold flash. (Refer JEDEC MS-012)
```

```
Publication Release Date: December 24, 2024
```
- 70 - Revision M

### 10.3 8 - Pad WSON 6x5-mm (Package Code P)

```
Symbol
```
```
Millimeters Inches
```
```
Min Nom Max Min Nom Max
A 0.70 0. 75 0.80 0.028 0.030 0.031
A1 0.00 0. 02 0.05 0.000 0.001 0.002
b 0.35 0.40 0.48 0.014 0.016 0.019
C^ --- 0.20 REF --- --- 0.008 REF ---
D 5.90 6.00 6.10 0.232 0.236 0.240
D2 3.35 3.40 3.45 0.132 0.134 0.136
E^ 4.90 5.00 5.10 0.193 0.197 0.201
E2 4.25 4.30 4.35 0.167 0.169 0.171
e^ 1.27 BSC 0.050 BSC
L 0.55 0.60 0.65 0.022 0.024 0.026
```
```
y 0.00 --- 0.075 0.000 --- 0.003
```
```
Note:
The metal pad area on the bottom center of the package is not connected to any internal electrical signals. It can be
left floating or connected to the device ground (GND pin). Avoid placement of exposed PCB vias under the pad.
```

##### - 71 -

### 10.4 8 - Pad WSON 8x6-mm (Package Code E)

##### SYMBOL

##### MILLIMETERS INCHES

```
Min Nom Max Min Nom Max
A 0.70 0.75 0.80 0.028 0.030 0.031
A1 0.00 0.02 0.05 0.000 0.001 0.002
b 0.35 0.40 0.48 0.014 0.016 0.019
C --- 0.20 Ref. --- --- 0.008 Ref. ---
D 7.90 8.00 8.10 0.311 0.315 0.319
D2 3.35 3.40 3.45 0.132 0.134 0.136
E 5.90 6.00 6.10 0.232 0.236 0.240
E2 4.25 4.30 4.35 0.167 0.169 0.171
e 1.27 BSC 0.050 BSC
L 0.45 0.50 0.55 0.018 0.020 0.022
```
```
y^ 0.00^ ---^ 0.05^ 0.000^ ---^ 0.002^
```
```
Note:
The metal pad area on the bottom center of the package is not connected to any internal electrical signals. It can be
left floating or connected to the device ground (GND pin). Avoid placement of exposed PCB vias under the pad.
```

```
Publication Release Date: December 24, 2024
```
- 72 - Revision M

### 10.5 24 - Ball TFBGA 8x6-mm (Package Code B, 5x5-1 ball array)

```
Symbol
```
```
Millimeters Inches
Min Nom Max Min Nom Max
A --- --- 1.20 --- --- 0.047
A1 0.25 0.30 0.35 0.010 0.012 0.014
A2 --- 0.85 --- --- 0.033 ---
b 0.35 0.40 0.45 0.014 0.016 0.018
D 7.90 8.00 8.10 0.311 0.315 0.319
D1 4.00 BSC 0.157 BSC
E 5.90 6.00 6.10 0.232 0.236 0.240
E1 4.00 BSC 0.157 BSC
SE 1.00 TYP 0.039 TYP
SD 1.00 TYP 0.039 TYP
```
```
e^ 1.00 BSC^ 0.039 BSC^
```
```
Note:
Ball land: 0.45mm. Ball Opening: 0.35mm
PCB ball land suggested <= 0.35mm
```

##### - 73 -

### 10.6 24 - Ball TFBGA 8x6-mm (Package Code C, 6x4 ball array)

```
Symbol
```
```
Millimeters Inches
Min Nom Max Min Nom Max
A --- --- 1.20 --- --- 0.047
A1 0.25 0.30 0.35 0.010 0.012 0.014
b 0.35 0.40 0.45 0.014 0.016 0.018
D 7.95 8.00 8.05 0.31 3 0.315 0.317
D1 5.00 BSC 0.197 BSC
E 5.95 6.00 6.05 0.234 0.236 0.238
E1 3.00 BSC 0.118 BSC
```
```
e^ 1.00 BSC^ 0.039 BSC^
```
```
Note:
Ball land: 0.45mm. Ball Opening: 0.35mm
PCB ball land suggested <= 0.35mm
```

```
Publication Release Date: December 24, 2024
```
- 74 - Revision M

### 10.7 24 - Ball WLCSP (Package Code Y)

```
Symbol
```
```
Millimeters Inches
Min Nom Max Min Nom Max
A 0.454 0.497 0.540 0.018 0.020 0.021
A1 0.152 0.167 0.182 0.006 0.007 0.007
C 0.302 0.330 0.358 0.012 0.013 0.014
D1 2.500 0.0984
E1 2.100 0.0827
eD 0.50 0.0197
eE 0.50 0.0197
b 0.240 0.300 0.360 0.009 0.012 0.014
aaa 0.10 0.004
bbb 0.10 0.004
ccc 0.03 0.001
ddd 0.15 0. 006
```
```
eee^ 0.05^ 0.002^
```
Notes:

1. Dimension b is measured at the maximum solder bump diameter, parallel to primary datum C.
    2. Dimension D/D2/D3 and E/E2/E3; please contact Winbond for details.


##### - 75 -

## 11. ORDERING INFORMATION

Notes:

1. The “W” prefix is not included on the part marking.
2. Only the 2nd letter is used for the part marking; WSON package type ZP & ZE are not used for the part marking.
3. Standard bulk shipments are in Tube (shape E). Please specify alternate packing method, such as Tape and Reel (shape
    T) or Tray (shape S), when placing orders.
4. For shipments with OTP feature enabled, please contact Winbond for details.
5. /HOLD function is disabled to support Standard, Dual and Quad I/O without user setting.
6. For DTR, QPI supporting, please refer to W25Q128JV-M DTR datasheet.

# W(1) 25Q 128 J V x(2) I

```
W = Winbond
```
```
25 Q = SpiFlash Serial Flash Memory with 4KB sectors, Dual/Quad I/O
```
```
128 J = 128 M-bit
```
```
V = 2.7V to 3.6V
```
```
S = 8-pin SOIC 208-mil F = 16-pin SOIC 300-mil
P = WSON8 6x5-mm E = WSON8 8x6-mm
```
#### B = TFBGA 8x6-mm (5x5-1 ball array) C = TFBGA 8x6-mm (6x4 ball array)

```
Y = 24 - ball WLCSP
```
```
I = Industrial (- 40 °C to +85°C) J = Industrial Plus(- 40 °C to + 10 5°C)
```
```
(3,4)
```
```
Q(5) = Green Package (Lead-free, RoHS Compliant, Halogen-free (TBBA), Antimony-Oxide-free Sb 2 O 3 )
with QE = 1 (fixed) in Status register-2. Backward compatible to FV family.
N(5) = Green Package (Lead-free, RoHS Compliant, Halogen-free (TBBA), Antimony-Oxide-free Sb 2 O 3 )
with QE = 1 (fixed) in Status register-2 & DRV=75%. Backward compatible to FV family.
M(6) = Green Package (Lead-free, RoHS Compliant, Halogen-free (TBBA), Antimony-Oxide-free Sb 2 O 3 )
with QE = 0 (programmable) in Status register-2. New device ID is used to identify JV family
```

```
Publication Release Date: December 24, 2024
```
- 76 - Revision M

### 11.1 Valid Part Numbers and Top Side Marking

The following table provides the valid part numbers for the W25Q128JV SpiFlash Memory. Please contact
Winbond for specific availability by density and package type. Winbond SpiFlash memories use a 12 - digit
Product Number for ordering. However, due to limited space, the Top Side Marking on all packages uses
an abbreviated 10 - digit number.

W25Q 128 JV-IQ valid part numbers:

#### W25Q 128 JV-IQ DENSITY PRODUCT NUMBER TOP SIDE MARKING

##### S

```
SOIC-8 208-mil
```
#### 128 M-bit W25Q128JVSIQ 25 Q 128 JVSQ

##### F

```
SOIC-16 300-mil
```
128 M-bit (^) W25Q128JVFIQ 25Q 128 JVFQ

##### P

#### WSON-8 6x5-mm 128 M-bit^ W25Q128JVPIQ^ 25Q^128 JVPQ^

##### E

```
WSON-8 8x6-mm
```
#### 128 M-bit W25Q128JVEIQ 25Q128JVEQ

##### B(1)

```
TFBGA-24 8x6-mm
(5x5 Ball Array)
```
#### 128 M-bit W25Q128JVBIQ 25Q128JVBQ

##### C(1)

```
TFBGA-24 8x6-mm
(6x4 Ball Array)
```
#### 128 M-bit W25Q128JVCIQ 25Q128JVCQ

##### Y(3)

```
24 - ball WLCSP
```
#### 128 M-bit W25Q128JVYIQ

#### Q128J

#### VYIQ

W25Q 128 JV-IN valid part numbers:

W25Q 128 JV-IN (^) DENSITY PRODUCT NUMBER TOP SIDE MARKING
S
SOIC-8 208-mil

#### 128 M-bit W25Q128JVSIN 25Q128JVSN

##### F

```
SOIC-16 300-mil
```
128 M-bit (^) W25Q128JVFIN 25Q128JVFN
P

#### WSON-8 6x5-mm 128 M-bit^ W25Q128JVPIN^ 25Q128JVPN^

##### E

```
WSON-8 8x6-mm
```
#### 128 M-bit W25Q128JVEIN 25Q128JVEN

```
Continued – next page
```

##### - 77 -

W25Q 128 JV-JQ valid part numbers:

W25Q128JV-JQ (^) DENSITY PRODUCT NUMBER TOP SIDE MARKING
S

#### SOIC-8 208-mil 128 M-bit^ W25Q128JVSJQ^ Q128JVSJQ^

##### F

```
SOIC-16 300-mil
```
128 M-bit (^) W25Q128JVFJQ Q128JVFJQ

##### P

```
WSON-8 6x5-mm
```
#### 128 M-bit W25Q128JVPJQ Q128JVPJQ

##### E

#### WSON-8 8x6-mm 128 M-bit^ W25Q128JVEJQ^ Q128JVEJQ^

##### B(1)

```
TFBGA-24 8x6-mm
(5x5 Ball Array)
```
#### 128 M-bit W25Q128JVBJQ Q128JVBJQ

##### C(1)

```
TFBGA-24 8x6-mm
(6x4 Ball Array)
```
#### 128 M-bit W25Q128JVCJQ Q128JVCJQ

W25Q128JV-IM(^2 ) valid part numbers:

#### W25Q128JV-IM DENSITY PRODUCT NUMBER TOP SIDE MARKING

##### S

#### SOIC-8 208-mil 128 M-bit^ W25Q128JVSIM^ 25Q128JVSM^

##### F

```
SOIC-16 300-mil
```
128 M-bit (^) W25Q128JVFIM 25Q128JVFM

##### P

#### WSON-8 6x5-mm 128 M-bit^ W25Q128JVPIM^ 25Q128JVPM^

##### E

```
WSON-8 8x6-mm
```
#### 128 M-bit W25Q128JVEIM 25Q128JVEM

##### B(1)

```
TFBGA-24 8x6-mm
(5x5 Ball Array)
```
#### 128 M-bit W25Q128JVBIM 25Q128JVBM

##### C(1)

```
TFBGA-24 8x6-mm
(6x4 Ball Array)
```
#### 128 M-bit W25Q128JVCIM 25Q128JVCM

```
Continued – next page
```

```
Publication Release Date: December 24, 2024
```
- 78 - Revision M

W25Q128JV-JM(^2 ) valid part numbers:

#### W25Q 1 28JV-JM DENSITY PRODUCT NUMBER TOP SIDE MARKING

##### S

#### SOIC-8 208-mil 128 M-bit^ W25Q128JVSJM^ Q128JVSJM^

##### F

```
SOIC-16 300-mil
```
128 M-bit (^) W25Q128JVFJM Q128JVFJM

##### P

#### WSON-8 6x5-mm 128 M-bit^ W25Q128JVPJM^ Q128JVPJM^

##### E

```
WSON-8 8x6-mm
```
#### 128 M-bit W25Q128JVEJM Q128JVEJM

##### B(1)

```
TFBGA-24 8x6-mm
(5x5 Ball Array)
```
#### 128 M-bit W25Q128JVBJM Q128JVBJM

##### C(1)

```
TFBGA-24 8x6-mm
(6x4 Ball Array)
```
#### 128 M-bit W25Q128JVCJM Q128JVCJM

Note:

1. These package types are special order, please contact Winbond for more information.
2. For DTR, QPI supporting, please refer to W25Q128JV-M DTR datasheet.
3. These package types are special order, please contact Winbond for more information


##### - 79 -

## 12. REVISION HISTORY

```
VERSION DATE PAGE DESCRIPTION
```
```
A 01/09/2015 New Create Datasheet
```
```
B 11/04/2016 Removed “Preliminary”
C 11/16/2016 12 Updated Status Register- 1
```
##### D 05/02/2017

##### 72 - 73

```
Updated /WP information
Updated W25Q128JV-IM order information
E 11/23/2017 8, 73- 75 Added WLCP information
```
##### F 2018/03/ 27

##### 73

##### 4,74- 76

##### 62,64- 65

```
Updated WLCSP E2 Drawing
Updated industrial plus information
Updated ICC3,tPP,tSLCH, tCHSL, tDVCH, tCHDX
```
```
G 2019/04/08 17 Updated SR-3 DRV0 typo
```
```
H 2021/03/10 16
16,17,75,75
```
```
Updated LB typo
Added DRV@75% information
```
```
I 2 021/08/23 75 Added W25Q128JVPIN
```
```
K 2 024/04/25 68 - 69 Added notice and drawing for SOP-208/300mil
L 2024/12/05 62 - 63 Added tPWD/VPDW Spec and waveform
```
```
M 2024/12/2 4 61 Updated Vcc range from 2.7~3.0v to 2.7~3.6v
```
## Trademarks

Winbond and SpiFlash are trademarks of Winbond Electronics Corporation.
All other marks are the property of their respective owner.

## Important Notice

Winbond products are not designed, intended, authorized or warranted for use as components in systems
or equipment intended for surgical implantation, atomic energy control instruments, airplane or spaceship
instruments, transportation instruments, traffic signal instruments, combustion control instruments, or for
other applications intended to support or sustain life. Furthermore, Winbond products are not intended for
applications wherein failure of Winbond products could result or lead to a situation wherein personal injury,
death or severe property or environmental damage could occur. Winbond customers using or selling these
products for use in such applications do so at their own risk and agree to fully indemnify Winbond for any
damages resulting from such improper use or sales.

Information in this document is provided solely in connection with Winbond products. Winbond
reserves the right to make changes, corrections, modifications or improvements to this document
and the products and services described herein at any time, without notice.


```
Publication Release Date: December 24, 2024
```
- 80 - Revision M


