This is information on a product in full production.

January 2024 DS10314 Rev 8 1/

# STM32F411xC STM32F411xE

## Arm

## ®

## Cortex

## ®

## -M4 32b MCU+FPU, 125 DMIPS, 512KB Flash,

## 128KB RAM, USB OTG FS, 11 TIMs, 1 ADC, 13 comm. interfaces

## Datasheet - production data

## Features

- Includes ST state-of-the-art patented
    technology
- Dynamic efficiency line with BAM (Batch
    acquisition mode)
    - 1.7 V to 3.6 V power supply
    - - 40°C to 85/105/125 °C temperature range
- Core: Arm® 32-bit Cortex®-M4 CPU with FPU,
    adaptive real-time accelerator (ART
    Accelerator) allowing 0-wait state execution
    from flash memory, frequency up to 100 MHz,
    memory protection unit,
    125 DMIPS/1.25 DMIPS/MHz (Dhrystone 2.1),
    and DSP instructions
- Memories
    - Up to 512 Kbytes of flash memory
    - 128 Kbytes of SRAM
- Clock, reset, and supply management
    - 1.7 V to 3.6 V application supply and I/Os
    - POR, PDR, PVD, and BOR
    - 4-to-26 MHz crystal oscillator
    - Internal 16 MHz factory-trimmed RC
    - 32 kHz oscillator for RTC with calibration
    - Internal 32 kHz RC with calibration
- Power consumption
    - Run: 100 μA/MHz (peripheral off)
    - Stop (Flash in Stop mode, fast wakeup
       time): 42 μA typical at 25 °C; 65 μA max at
       25 °C
    - Stop (Flash in Deep power down mode,
       slow wakeup time): down to 9 μA at 25 °C;
       28 μA max at 25 °C
    - Standby: 1.8 μA at 25 °C / 1.7 V without
       RTC; 11 μA at 85 °C at 1.7 V
    –VBAT supply for RTC: 1 μA at 25 °C
- 1×12-bit, 2.4 MSPS A/D converter: up to 16
    channels
- General-purpose DMA: 16-stream DMA
    controllers with FIFOs and burst support
- Up to 11 timers: up to six 16-bit, two 32-bit
    timers up to 100 MHz, each with up to four
    IC/OC/PWM or pulse counter and quadrature
    (incremental) encoder input, two watchdog

```
timers (independent and window) and a
SysTick timer
```
- Debug mode
    - Serial wire debug (SWD) & JTAG
       interfaces
    –Cortex®-M4 Embedded Trace Macrocell™
- Up to 81 I/O ports with interrupt capability
    - Up to 78 fast I/Os up to 100 MHz
    - Up to 77 5 V-tolerant I/Os
- Up to 13 communication interfaces
    - Up to 3 x I^2 C interfaces (SMBus/PMBus)
    - Up to 3 USARTs (2 x 12.5 Mbit/s,
       1 x 6.25 Mbit/s), ISO 7816 interface, LIN,
       IrDA, modem control)
    - Up to 5 SPI/I2Ss (up to 50 Mbit/s, SPI, or
       I2S audio protocol), SPI2 and SPI3 with
       muxed full-duplex I^2 S to achieve audio
       class accuracy via internal audio PLL or
       external clock
    - SDIO interface (SD/MMC/eMMC)
    - Advanced connectivity: USB 2.0 full-speed
       device/host/OTG controller with on-chip
       PHY
- CRC calculation unit
- 96-bit unique ID
- RTC: subsecond accuracy, hardware calendar
- All packages are ECOPACK2 compliant

```
Table 1. Device summary
```
```
Reference Part number
```
```
STM32F411xC STM32F411CC, STM32F411RC, STM32F411VC
```
```
STM32F411xE STM32F411CE, STM32F411RE, STM32F411VE
```
```
WLCSP
```
```
UFBGA
```
```
UFQFPN
(7 × 7 mm) UFBGA
(7 × 7 mm)
```
```
(2.999x3.185 mm)
```
```
LQFP
(14 × 14 mm)
LQFP
(10x10 mm)
```
```
http://www.st.com
```

```
STM32F411xC STM32F411xE
```
2/151 DS10314 Rev 8

### Application

- Motor drive and application control
- Medical equipment
- Industrial applications: PLC, inverters, circuit breakers
- Printers, and scanners
- Alarm systems, video intercom, and HVAC
- Home audio appliances
- Mobile phone sensor hub


## STM32F411xC STM32F411xE Contents








         - DS10314 Rev 8 3/
            -
- 1 Introduction Contents
- 2 Description
   - 2.1 Compatibility with STM32F4 series
- 3 Functional overview
      - flash memory and SRAM 3.1 Arm® Cortex®-M4 with FPU core with embedded
   - 3.2 Adaptive real-time memory accelerator (ART Accelerator)
   - 3.3 Batch acquisition mode (BAM)
   - 3.4 Memory protection unit
   - 3.5 Embedded flash memory
   - 3.6 CRC (cyclic redundancy check) calculation unit
   - 3.7 Embedded SRAM
   - 3.8 Multi-AHB bus matrix
   - 3.9 DMA controller (DMA)
   - 3.10 Nested vectored interrupt controller (NVIC)
   - 3.11 External interrupt/event controller (EXTI)
   - 3.12 Clocks and startup
   - 3.13 Boot modes
   - 3.14 Power supply schemes
   - 3.15 Power supply supervisor
      - 3.15.1 Internal reset ON
      - 3.15.2 Internal reset OFF
   - 3.16 Voltage regulator
      - 3.16.1 Regulator ON
      - 3.16.2 Regulator OFF
      - 3.16.3 Regulator ON/OFF and internal power supply supervisor availability
   - 3.17 Real-time clock (RTC) and backup registers
   - 3.18 Low-power modes
   - 3.19 VBAT operation
   - 3.20 Timers and watchdogs
- 4/151 DS10314 Rev Contents STM32F411xC STM32F411xE
      - 3.20.1 Advanced-control timers (TIM1)
      - 3.20.2 General-purpose timers (TIMx)
      - 3.20.3 Independent watchdog
      - 3.20.4 Window watchdog
      - 3.20.5 SysTick timer
   - 3.21 Inter-integrated circuit interface (I2C)
   - 3.22 Universal synchronous/asynchronous receiver transmitters (USART)
   - 3.23 Serial peripheral interface (SPI)
   - 3.24 Inter-integrated sound (I^2 S)
   - 3.25 Audio PLL (PLLI2S)
   - 3.26 Secure digital input/output interface (SDIO)
   - 3.27 Universal serial bus on-the-go full-speed (OTG_FS)
   - 3.28 General-purpose input/outputs (GPIOs)
   - 3.29 Analog-to-digital converter (ADC)
   - 3.30 Temperature sensor
   - 3.31 Serial wire JTAG debug port (SWJ-DP)
   - 3.32 Embedded Trace Macrocell™
- 4 Pinouts and pin description
- 5 Memory mapping
- 6 Electrical characteristics
   - 6.1 Parameter conditions
      - 6.1.1 Minimum and maximum values
      - 6.1.2 Typical values
      - 6.1.3 Typical curves
      - 6.1.4 Loading capacitor
      - 6.1.5 Pin input voltage
      - 6.1.6 Power supply scheme
      - 6.1.7 Current consumption measurement
   - 6.2 Absolute maximum ratings
   - 6.3 Operating conditions
      - 6.3.1 General operating conditions
      - 6.3.2 VCAP_1/VCAP_2 external capacitors
      - 6.3.3 Operating conditions at power-up/power-down (regulator ON)
         - DS10314 Rev 8 5/
            - STM32F411xC STM32F411xE Contents
      - 6.3.4 Operating conditions at power-up / power-down (regulator OFF)
      - 6.3.5 Embedded reset and power control block characteristics
      - 6.3.6 Supply current characteristics
      - 6.3.7 Wakeup time from low-power modes
      - 6.3.8 External clock source characteristics
      - 6.3.9 Internal clock source characteristics
      - 6.3.10 PLL characteristics
      - 6.3.11 PLL spread spectrum clock generation (SSCG) characteristics
      - 6.3.12 Memory characteristics
      - 6.3.13 EMC characteristics
      - 6.3.14 Absolute maximum ratings (electrical sensitivity)
      - 6.3.15 I/O current injection characteristics
      - 6.3.16 I/O port characteristics
      - 6.3.17 NRST pin characteristics
      - 6.3.18 TIM timer characteristics
      - 6.3.19 Communications interfaces
      - 6.3.20 12-bit ADC characteristics
      - 6.3.21 Temperature sensor characteristics
      - 6.3.22 VBAT monitoring characteristics
      - 6.3.23 Embedded reference voltage
      - 6.3.24 SD/SDIO MMC/eMMC card host interface (SDIO) characteristics
      - 6.3.25 RTC characteristics
- 7 Package information
   - 7.1 Device marking
   - 7.2 WLCSP49 package information (A0ZV)
   - 7.3 UFQFPN48 package information (A0B9)
   - 7.4 LQFP64 package information (5W)
   - 7.5 LQFP100 package information (1L)
   - 7.6 UFBGA100 package information (A0C2)
   - 7.7 Thermal characteristics
      - 7.7.1 Reference document
- 8 Ordering information
- Appendix A Recommendations when using the internal reset OFF
- 6/151 DS10314 Rev Contents STM32F411xC STM32F411xE
   - A.1 Operating conditions
- Appendix B Application block diagrams
   - B.1 USB OTG Full Speed (FS) interface solutions
   - B.2 Sensor Hub application example.
   - B.3 Batch Acquisition Mode (BAM) example
- 9 Important security notice
- Revision history
      - DS10314 Rev 8 7/
         - STM32F411xC STM32F411xE List of tables
- Table 1. Device summary List of tables
- Table 2. STM32F411xC/xE features and peripheral counts
- Table 3. Regulator ON/OFF and internal power supply supervisor availability.
- Table 4. Timer feature comparison
- Table 5. Comparison of I2C analog and digital filters
- Table 6. USART feature comparison
- Table 7. Legend/abbreviations used in the pinout table
- Table 8. STM32F411xC/xE pin definitions
- Table 9. Alternate function mapping
   - register boundary addresses Table 10. STM32F411xC/xE
- Table 11. Voltage characteristics
- Table 12. Current characteristics
- Table 13. Thermal characteristics.
- Table 14. General operating conditions
- Table 15. Features depending on the operating power supply range
- Table 16. VCAP_1/VCAP_2 operating conditions
- Table 17. Operating conditions at power-up / power-down (regulator ON)
- Table 18. Operating conditions at power-up / power-down (regulator OFF).
- Table 19. Embedded reset and power control block characteristics.
   - accelerator disabled) running from SRAM - VDD = 1.7 V Table 20. Typical and maximum current consumption, code with data processing (ART
   - accelerator disabled) running from SRAM - VDD = 3.6 V Table 21. Typical and maximum current consumption, code with data processing (ART
   - (ART accelerator enabled except prefetch) running from flash memory- VDD = 1.7 V Table 22. Typical and maximum current consumption in run mode, code with data processing
   - (ART accelerator enabled except prefetch) running from flash memory - VDD = 3.6 V Table 23. Typical and maximum current consumption in run mode, code with data processing
   - (ART accelerator disabled) running from flash memory - VDD = 3.6 V Table 24. Typical and maximum current consumption in run mode, code with data processing
   - (ART accelerator enabled with prefetch) running from flash memory - VDD = 3.6 V Table 25. Typical and maximum current consumption in run mode, code with data processing
- Table 26. Typical and maximum current consumption in Sleep mode - VDD = 3.6 V
- Table 27. Typical and maximum current consumptions in Stop mode - VDD = 1.7 V
- Table 28. Typical and maximum current consumption in Stop mode - VDD=3.6 V
- Table 29. Typical and maximum current consumption in Standby mode - VDD= 1.7 V
- Table 30. Typical and maximum current consumption in Standby mode - VDD= 3.6 V
- Table 31. Typical and maximum current consumptions in VBAT mode.
- Table 32. Switching output I/O current consumption
- Table 33. Peripheral current consumption
- Table 34. Low-power mode wakeup timings(1).
- Table 35. High-speed external user clock characteristics.
- Table 36. Low-speed external user clock characteristics
- Table 37. HSE 4-26 MHz oscillator characteristics.
- Table 38. LSE oscillator characteristics (fLSE = 32.768 kHz)
- Table 39. HSI oscillator characteristics
- Table 40. LSI oscillator characteristics
- Table 41. Main PLL characteristics.
- 8/151 DS10314 Rev List of tables STM32F411xC STM32F411xE
- Table 42. PLLI2S (audio PLL) characteristics
- Table 43. SSCG parameter constraints
- Table 44. Flash memory characteristics
- Table 45. Flash memory programming
- Table 46. Flash memory programming with VPP voltage
- Table 47. Flash memory endurance and data retention
- Table 48. EMS characteristics for LQFP100 package
- Table 49. EMI characteristics for LQFP100
- Table 50. ESD absolute maximum ratings
- Table 51. Electrical sensitivities
- Table 52. I/O current injection susceptibility
- Table 53. I/O static characteristics
- Table 54. Output voltage characteristics
- Table 55. I/O AC characteristics
- Table 56. NRST pin characteristics
- Table 57. TIMx characteristics
- Table 58. I^2 C characteristics.
- Table 59. SCL frequency (fPCLK1= 50 MHz, VDD = VDD_I2C = 3.3 V)
- Table 60. SPI dynamic characteristics
- Table 61. I^2 S dynamic characteristics
- Table 62. USB OTG FS startup time
- Table 63. USB OTG FS DC electrical characteristics.
- Table 64. USB OTG FS electrical characteristics
- Table 65. ADC characteristics
- Table 66. ADC accuracy at fADC = 18 MHz
- Table 67. ADC accuracy at fADC = 30 MHz
- Table 68. ADC accuracy at fADC = 36 MHz
- Table 69. ADC dynamic accuracy at fADC = 18 MHz - limited test conditions
- Table 70. ADC dynamic accuracy at fADC = 36 MHz - limited test conditions
- Table 71. Temperature sensor characteristics
- Table 72. Temperature sensor calibration values.
- Table 73. VBAT monitoring characteristics
- Table 74. Embedded internal reference voltage
- Table 75. Internal reference voltage calibration values
- Table 76. Dynamic characteristics: SD / MMC characteristics
- Table 77. Dynamic characteristics: eMMC characteristics VDD = 1.7 V to 1.9 V
- Table 78. RTC characteristics
- Table 79. WLCSP49 - Mechanical data
- Table 80. WLCSP49 - Example of PCB design rules (0.4 mm pitch)
- Table 81. UFQFPN48 – Mechanical data
- Table 82. LQFP64 - Mechanical data
- Table 83. LQFP100 - Mechanical data
- Table 84. UFBGA100 - Mechanical data
- Table 85. UFBGA100 - Example of PCB design rules (0.5 mm pitch BGA)
- Table 86. Package thermal characteristics
- Table 87. Ordering information scheme
- Table 88. Limitations depending on the operating power supply range
- Table 89. Document revision history
      - DS10314 Rev 8 9/
         - STM32F411xC STM32F411xE List of figures
- Figure 1. Compatible board design for LQFP100 package List of figures
- Figure 2. Compatible board design for LQFP64 package
- Figure 3. STM32F411xC/xE block diagram
- Figure 4. Multi-AHB matrix
- Figure 5. Power supply supervisor interconnection with internal reset OFF
- Figure 6. Regulator OFF
   - power-down reset risen after VCAP_1/VCAP_2 stabilization. Figure 7. Startup in regulator OFF: slow VDD slope -
   - power-down reset risen before VCAP_1/VCAP_2 stabilization Figure 8. Startup in regulator OFF mode: fast VDD slope -
- Figure 9. STM32F411xC/xE WLCSP49 pinout
- Figure 10. STM32F411xC/xE UFQFPN48 pinout
- Figure 11. STM32F411xC/xE LQFP64 pinout
- Figure 12. STM32F411xC/xE LQFP100 pinout
- Figure 13. STM32F411xC/xE UFBGA100 pinout
- Figure 14. Memory map
- Figure 15. Pin loading conditions
- Figure 16. Input voltage measurement
- Figure 17. Power supply scheme
- Figure 18. Current consumption measurement scheme
- Figure 19. External capacitor CEXT
- Figure 20. Typical VBAT current consumption (LSE in low-drive mode and RTC ON).
- Figure 21. Low-power mode wakeup
- Figure 22. High-speed external clock source AC timing diagram
- Figure 23. Low-speed external clock source AC timing diagram
- Figure 24. Typical application with an 8 MHz crystal
- Figure 25. Typical application with a 32.768 kHz crystal
- Figure 26. ACCHSI versus temperature
- Figure 27. ACCLSI versus temperature
- Figure 28. PLL output clock waveforms in center spread mode
- Figure 29. PLL output clock waveforms in down spread mode
- Figure 30. FT/TC I/O input characteristics
- Figure 31. I/O AC characteristics definition
- Figure 32. Recommended NRST pin protection
- Figure 33. I^2 C bus AC waveforms and measurement circuit
- Figure 34. SPI timing diagram - slave mode and CPHA =
- Figure 35. SPI timing diagram - slave mode and CPHA = 1(1)
- Figure 36. SPI timing diagram - master mode(1)
- Figure 37. I^2 S slave timing diagram (Philips protocol)(1)
- Figure 38. I^2 S master timing diagram (Philips protocol)(1).
- Figure 39. USB OTG FS timings: definition of data signal rise and fall time
- Figure 40. ADC accuracy characteristics
- Figure 41. Typical connection diagram using the ADC
- Figure 42. Power supply and reference decoupling (VREF+ not connected to VDDA).
- Figure 43. Power supply and reference decoupling (VREF+ connected to VDDA).
- Figure 44. SDIO high-speed mode
- Figure 45. SD default mode
- Figure 46. WLCSP49 - Outline
- 10/151 DS10314 Rev List of figures STM32F411xC STM32F411xE
- Figure 47. WLCSP49 - Footprint example
- Figure 48. WLCSP49 marking (package top view)
- Figure 49. UFQFPN48 – Outline
- Figure 50. UFQFPN48 – Footprint example
- Figure 51. LQFP64 - Outline(15).
- Figure 52. LQFP64 - Footprint example
- Figure 53. LQFP100 - Outline(15).
- Figure 54. LQFP100 - Footprint example
- Figure 55. UFBGA100 - Outline(13)
- Figure 56. UFBGA100 - Footprint example
- Figure 57. USB controller configured as peripheral-only and used in Full-Speed mode
- Figure 58. USB controller configured as host-only and used in Full-Speed mode.
- Figure 59. USB controller configured in dual mode and used in Full-Speed mode
- Figure 60. Sensor Hub application example
- Figure 61. Batch Acquisition Mode (BAM) example


```
DS10314 Rev 8 11/
```
**STM32F411xC STM32F411xE Introduction**

```
57
```
## 1 Introduction Contents

```
This document provides the ordering information and mechanical device characteristics of
the STM32F411xC/xE microcontrollers.
```
```
This document has to be read with RM0383 reference manual, which is available from the
STMicroelectronics website http://www.st.com. It includes all information concerning flash
memory programming.
```
```
For information on the Arm®(a) Cortex®-M33 core, refer to the Cortex®-M33 Technical
Reference Manual, available from the http://www.arm.com website.
```
```
a. Arm is a registered trademark of Arm Limited (or its subsidiaries) in the US and/or elsewhere.
```

**Description STM32F411xC STM32F411xE**

12/151 DS10314 Rev 8

## 2 Description

```
The STM32F411XC/XE devices are based on the high-performance Arm®Cortex®-M4 32-
bit RISC core operating at a frequency of up to 100 MHz.
The Cortex®-M4 core features a floating-point unit (FPU) single precision, which supports all
Arm single-precision data-processing instructions and data types. It also implements a full
set of DSP instructions and a memory protection unit (MPU), which enhances application
security.
```
```
The STM32F411xC/xE belongs to the STM32 Dynamic Efficiency product line (with
products combining power efficiency, performance and integration) while adding a new
innovative feature called Batch Acquisition Mode (BAM) allowing to save even more power
consumption during data batching.
```
```
The STM32F411xC/xE incorporate high-speed embedded memories (up to 512 Kbytes of
flash memory, 128 Kbytes of SRAM), and an extensive range of enhanced I/Os and
peripherals connected to two APB buses, two AHB bus and a 32-bit multi-AHB bus matrix.
```
```
All devices offer one 12-bit ADC, a low-power RTC, six general-purpose 16-bit timers
including one PWM timer for motor control, two general-purpose 32-bit timers. They also
feature standard and advanced communication interfaces.
```
- Up to three I^2 Cs
- Five SPIs
- Five I^2 Ss out of which two are full duplex. To achieve audio class accuracy, the I^2 S
    peripherals can be clocked via a dedicated internal audio PLL or via an external clock
    to allow synchronization.
- Three USARTs
- SDIO interface
- USB 2.0 OTG full speed interface

```
The STM32F411xC/xE operate in the - 40 to + 125 °C temperature range from a 1.7 (PDR
OFF) to 3.6 V power supply. A comprehensive set of power-saving mode allows the design
of low-power applications.
```

```
DS10314 Rev 8 13/
```
**STM32F411xC STM32F411xE Description**

```
57
```
## Table 2. STM32F411xC/xE features and peripheral counts

```
Peripherals STM32F411xC STM32F411xE
```
```
Flash memory in Kbytes 256 512
```
```
SRAM in Kbytes System 128
```
```
Timers
```
```
General-
purpose^7
```
```
Advanced-
control^1
```
```
Communication
interfaces
```
```
SPI/ I^2 S 5/5 (2 full duplex)
```
```
I^2 C
```
```
USART 3
```
```
SDIO 1
```
```
USB OTG FS 1
```
```
GPIOs 36 50 81 36 50 81
```
```
12-bit ADC
Number of channels
```
##### 1

##### 10 16 10 16

```
Maximum CPU frequency 100 MHz
```
```
Operating voltage 1.7 to 3.6 V
```
```
Operating temperatures
```
```
Ambient temperatures: - 40 to +85 °C / - 40 to + 105 °C/ - 40 to + 125 °C
```
```
Junction temperature: – 40 to + 130 °C
```
```
Package
```
##### WLCSP

##### UFQFPN

##### LQFP

##### UFBGA

##### LQFP

##### WLCSP

##### UFQFPN

##### LQFP

##### UFBGA

##### LQFP


**Description STM32F411xC STM32F411xE**

14/151 DS10314 Rev 8

### 2.1 Compatibility with STM32F4 series

```
The STM32F411xC/xE are fully software and feature compatible with the STM32F4 series
(STM32F42x, STM32F401, STM32F43x, STM32F41x, STM32F405 and STM32F407)
```
```
The STM32F411xC/xE can be used as a drop-in replacement of the other STM32F
products but some slight changes have to be done on the PCB board.
```
```
Figure 1. Compatible board design for LQFP100 package
```
```
MSv37802V
```
```
58
57
56
55
54
53
52
51
```
```
PD
PD
PD
PD
PB
PB
PB
PB
```
```
PE10PE11PE12PE13PE14PE15PB
VCAP_
```
```
VSSVDD
```
```
41424344454647
```
```
PB11 not available anymore
Replaced by VCAP_
```
```
58
57
56
55
54
53
52
51
```
```
PD
PD
PD
PD
PB
PB
PB
PB
```
```
PE10PE11PE12PE13PE14PE15PB
VCAP_
```
```
VDD
```
```
4142434445464748
```
```
STM32F405/STM32F415 line
STM32F407/STM32F417 line
STM32F427/STM32F437 line
STM32F429/STM32F439 line
```
```
VSSVDD
```
```
PB
```
(^4950484950)
STM32F401xx
STM32F410xx
STM32F411xx
STM32F412xx
STM32F413xx
STM32F423xx
STM32F446xx
VSSVDD


```
DS10314 Rev 8 15/
```
**STM32F411xC STM32F411xE Description**

```
57
```
## Figure 2. Compatible board design for LQFP64 package

```
MSv37803V
```
```
VCAP_1 increased to 4.7 μf
(65RUEHORZ
```
```
670)670)OLQH
```
```
VSS
```
```
VSS
```
```
VDD
```
```
VDD
```
```
53 5 2 5 1 50 49 48
47
46
45
44
43
42
41
40
39
38
37
36
35
34
28 29 30 31 3233
```
```
PC12PC11PC10PA15PA
VDD
VCAP_
PA
PA
PA
PA
PA
PA
PC
PC
PC
PC
PB
PB
PB
PB
```
```
PB2PB
VCAP_
```
```
PB11VDD
```
```
3%QRWDYDLODEOHDQ\PRUH
5HSODFHGE\9CAP_
```
```
53 5 2 5 1 50 49 48
47
46
45
44
43
42
41
40
39
38
37
36
35
34
33
28 3132
```
```
PC12PC11PC10PA15PA
VDD
VSS
PA
PA
PA
PA
PA
PA
PC
PC
PC
PC
PB
PB
PB
PB
```
```
PB2PB
VCAP_
```
```
VSSVDD
```
```
VSSVDD
```
```
VSS
```
```
VDD
```
```
STM32F401xx
STM32F410xx
STM32F411xx
STM32F412xx
STM32F413xx
STM32F423xx
STM32F446xx
```
```
2930
```

**Description STM32F411xC STM32F411xE**

16/151 DS10314 Rev 8

## Figure 3. STM32F411xC/xE block diagram

1. The timers connected to APB2 are clocked from TIMxCLK up to 100 MHz, while the timers connected to APB1 are clocked
    from TIMxCLK up to 100 MHz.

```
MSv34920V
```
```
GPIO PORT A
```
```
AHB/ APB
```
```
up to 81 AF EXT IT. WKUP
```
```
PA[15:0]
```
```
TIM1 / PWM
```
```
3 compl. channels TIM1_CH1[1:3]N,
4 channels TIM1_CH1[1:4]ETR,
BKIN as AF
```
```
CTS, RTS as AFRX, TX, CK, USART
```
```
SPI1/I2S
```
```
APB2 60 MHz
```
```
APB1 30MHz
```
```
16 analog inputs
```
```
VDDREF_ADC
```
```
SP3/I2S3 MOSI/SD, MISO/SD_ext,SCK/CK, NSS/WS,
```
```
ALARM_OUT
```
```
OSC32_IN
OSC32_OUT
```
```
VDDA, VSSA
NRST
```
```
smcard
irDA
```
```
16b
```
```
VBAT = 1.65 to 3.6 V
```
```
DMA
```
```
I2C3/SMBUS SCL, SDA, SMBA as AF
```
```
JTAG & SW
```
```
ARM Cortex-M
100 MHz
```
```
ETM NVIC
```
```
MPU
```
```
TRACED[3:0]TRACECLK
```
```
DMA
8 StreamsFIFO
```
```
ACCEL/CACHE
```
```
AHB1 100 MHz
```
```
USAR T 2MBpsTemperature sensor
ADC1 IF
@VDDA
```
```
POR/PDR
BOR
```
```
Supply
supervision
```
```
@ V DDA
```
```
PVD
```
```
Int
```
```
POR
reset
```
```
XTAL 32 kHz
```
```
MAN AGT
```
```
RTC
```
```
RC HS
RC L S
```
```
PWR
interface
```
```
WDG 32K
```
```
@VBAT
```
```
@VDDA
```
```
@VDD
```
```
AWU
```
```
Reset &
clock
control
```
```
P L L1&
```
```
APB2CLK
```
```
VDD = 1.7 to 3.6 V
```
```
VSS
VCAP
```
```
regulatorVoltage
3.3 to 1.2 V
```
```
VDD Power managmt
```
```
@VDD
```
```
Backup register STAMP
```
```
AHB bus-matrix 7S4M
```
```
APB2 100 MHz
```
```
LS
```
```
2 channels as AF TIM
```
```
512 KB Flash
```
```
TIM
TIM
TIM
TIM
```
```
D-BUS
```
```
FPU
```
```
APB1 50 MHz (max)
```
```
AHB2 100 MHz
```
```
NJTRST, JTDI,
JTDO/SWD, JTDOJTCK/SWCLK
```
```
I-BUS
S-BUS
```
```
DMA
```
```
8 StreamsFIFO
```
```
PB[15:0]
PC[15:0]
```
```
PH[1:0]
```
```
GPIO PORT B
GPIO PORT C
```
```
GPIO PORT H
```
```
16b
TIM10 16b
TIM11 16b
```
```
smcard
irDA USART
```
```
1 channel as AF
1 channel as AF
```
```
RX, TX, CK as AF
I2C2/SMBUS
```
```
I2C1/SMBUS
```
```
SCL, SDA, SMBA as AF
```
```
SCL, SDA, SMBA as AF
```
```
SP2/I2S2 MOSI/SD, MISO/SD_ext, SCK/CK, NSS/WS,
```
```
RX, TX as AF
USART2 CTS, RTS as AF
smcard
irDA
```
```
32b
```
```
16b
```
```
16b
```
```
32b
```
```
4 channels
```
```
4 channels, ETR as AF
```
```
4 channels, ETR as AF
```
```
4 channels, ETR as AF
```
```
DMA
AHB/ APB
```
```
LS
```
```
OSC_IN
OSC_OUT
```
```
HCLK
```
```
XTAL OSC4- 16MHz
```
```
128 KB SRAM
```
```
WWDG
```
```
APB1CLKAHB2PCLKAHB1PCLK
```
```
CRC
```
```
(PDR OFF)
1.8 to 3.6 V
(PDR ON)
```
```
CMD, CK as AFD[7:0] SDIO / MMC FIFO
```
```
USB
PHYOTG FS FIFO
```
```
DP
DM
ID, VBUS, SOF
```
```
MOSI/SD, MISO, SCK/CK, NSS/WS as AF SPI4/I2S
```
```
PD[15:0] GPIO PORT D
PE[15:0] GPIO PORT E
```
```
MOSI/SD, MISO, SCK/CK,NSS/WS as AF
```
```
MOSI/SD, MISO, SCK/CK, NSS/WS as AF SPI5/I2S
```
```
MCK as AF
```
```
MCK as AF
```

```
DS10314 Rev 8 17/
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
## 3 Functional overview

### 3.1 Arm

**®**

### Cortex

**®**

### -M4 with FPU core with embedded

### flash memory and SRAM

```
The Arm®Cortex®-M4 with FPU processor is the latest generation of Arm processors for
embedded systems. It was developed to provide a low-cost platform that meets the needs of
MCU implementation, with a reduced pin count and low-power consumption, while
delivering outstanding computational performance and an advanced response to interrupts.
```
```
The Arm®Cortex®-M4 with FPU 32-bit RISC processor features exceptional code-
efficiency, delivering the high-performance expected from an Arm core in the memory size
usually associated with 8- and 16-bit devices. The processor supports a set of DSP
instructions, which allow efficient signal processing and complex algorithm execution. Its
single precision FPU (floating-point unit) speeds up software development by using
metalanguage development tools, while avoiding saturation.
```
```
The STM32F411xC/xE devices are compatible with all Arm tools and software.
```
```
Figure 3 shows the general block diagram of the STM32F411xC/xE.
```
_Note: Cortex_ ® _-M4 with FPU is binary compatible with Cortex_ ® _-M3._

### 3.2 Adaptive real-time memory accelerator (ART Accelerator)

```
The ART Accelerator is a memory accelerator, which is optimized for STM32 industry-
standard Arm® Cortex®-M4 with FPU processors. It balances the inherent performance
advantage of the Arm® Cortex®-M4 with FPU over flash memory technologies, which
normally requires the processor to wait for the flash memory at higher frequencies.
```
```
To release the processor full 105 DMIPS performance at this frequency, the accelerator
implements an instruction prefetch queue and branch cache, which increases program
execution speed from the -bit flash memory. Based on CoreMark benchmark, the
performance achieved thanks to the ART Accelerator is equivalent to 0 wait state program
execution from flash memory at a CPU frequency up to 100 MHz.
```
### 3.3 Batch acquisition mode (BAM)

```
The batch acquisition mode allows enhanced power efficiency during data batching. It
enables data acquisition through any communication peripherals directly to memory using
the DMA in reduced power consumption as well as data processing while the rest of the
system is in low-power mode (including the flash and ART). For example in an audio
system, a smart combination of PDM audio sample acquisition and processing from the I2S
directly to RAM (flash and ART™ stopped) with the DMA using BAM followed by some very
short processing from flash allows to drastically reduce the power consumption of the
application. A dedicated application note (AN4515) describes how to implement the BAM to
allow the best power efficiency.
```

**Functional overview STM32F411xC STM32F411xE**

18/151 DS10314 Rev 8

### 3.4 Memory protection unit

```
The memory protection unit (MPU) is used to manage the CPU accesses to memory to
prevent one task to accidentally corrupt the memory or resources used by any other active
task. This memory area is organized into up to eight protected areas that can in turn be
divided up into eight subareas. The protection area sizes are between 32 bytes and the
whole 4 gigabytes of addressable memory.
```
```
The MPU is especially helpful for applications where some critical or certified code has to be
protected against the misbehavior of other tasks. It is usually managed by an RTOS (real-
time operating system). If a program accesses a memory location that is prohibited by the
MPU, the RTOS can detect it and take action. In an RTOS environment, the kernel can
dynamically update the MPU area setting, based on the process to be executed.
```
```
The MPU is optional and can be bypassed for applications that do not need it.
```
### 3.5 Embedded flash memory

```
The devices embed up to 512 Kbytes of flash memory available for storing programs and
data.
```
```
To optimize the power consumption the flash memory can also be switched off in Run or in
Sleep mode (see Section 3.18: Low-power modes ). Two modes are available: Flash in Stop
mode or in DeepSleep mode (trade off between power saving and startup time, see
Table 34: Low-power mode wakeup timings(1) ). Before disabling the flash memory, the code
must be executed from the internal RAM. One-time programmable bytes
```
```
A one-time programmable area is available with 16 OTP blocks of 32 bytes. Each block can
be individually locked.
```
```
(Additional information can be found in the product reference manual.)
```
### 3.6 CRC (cyclic redundancy check) calculation unit

```
The CRC (cyclic redundancy check) calculation unit is used to get a CRC code from a 32-bit
data word and a fixed generator polynomial.
```
```
Among other applications, CRC-based techniques are used to verify data transmission or
storage integrity. In the scope of the EN/IEC 60335-1 standard, they offer a means of
verifying the flash memory integrity. The CRC calculation unit helps compute a software
signature during runtime, to be compared with a reference signature generated at link-time
and stored at a given memory location.
```
### 3.7 Embedded SRAM

```
All devices embed:
```
- 128 Kbytes of system SRAM, which can be accessed (read/write) at CPU clock speed
    with 0 wait states


```
DS10314 Rev 8 19/
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
### 3.8 Multi-AHB bus matrix

```
The 32-bit multi-AHB bus matrix interconnects all the masters (CPU, DMAs) and the slaves
(flash memory, RAM, AHB and APB peripherals) and ensures a seamless and efficient
operation even when several high-speed peripherals work simultaneously.
```
## Figure 4. Multi-AHB matrix

### 3.9 DMA controller (DMA)

```
The devices feature two general-purpose dual-port DMAs (DMA1 and DMA2) with 8
streams each. They are able to manage memory-to-memory, peripheral-to-memory, and
memory-to-peripheral transfers. They feature dedicated FIFOs for APB/AHB peripherals,
support burst transfer and are designed to provide the maximum peripheral bandwidth
(AHB/APB).
```
```
The two DMA controllers support circular buffer management, so that no specific code is
needed when the controller reaches the end of the buffer. The two DMA controllers also
have a double buffering feature, which automates the use and switching of two memory
buffers without requiring any special code.
```
```
Each stream is connected to dedicated hardware DMA requests, with support for software
trigger on each stream. Configuration is made by software and transfer sizes between
source and destination are independent.
```
```
ARM
Cortex-M
```
```
GP
DMA
```
```
GP
DMA
```
```
Bus matrix-S
```
```
S0 S1 S2 S3 S4 S
ICODE
```
```
DCODE ACCEL
```
```
Flash
512 kB
```
```
SRAM
128 Kbytes
```
```
AHB
periph
```
```
M
```
```
M
```
```
M
```
```
M
```
```
I-bus D-busS-bus
DMA_PI
DMA_MEM
DMA_MEM
```
```
DMA_P
```
```
MS34921V
```
```
M3 periph1AHB APB
```
```
APB
```

**Functional overview STM32F411xC STM32F411xE**

20/151 DS10314 Rev 8

```
The DMA can be used with the main peripherals:
```
- SPI and I^2 S
- I^2 C
- USART
- General-purpose, basic and advanced-control timers TIMx
- SD/SDIO/MMC/eMMC host interface
- ADC

### 3.10 Nested vectored interrupt controller (NVIC)

```
The devices embed a nested vectored interrupt controller able to manage 16 priority levels,
and handle up to 62 maskable interrupt channels plus the 16 interrupt lines of the Cortex®-
M4 with FPU.
```
- Closely coupled NVIC gives low-latency interrupt processing
- Interrupt entry vector table address passed directly to the core
- Allows early processing of interrupts
- Processing of late arriving, higher-priority interrupts
- Support tail chaining
- Processor state automatically saved
- Interrupt entry restored on interrupt exit with no instruction overhead

```
This hardware block provides flexible interrupt management features with minimum interrupt
latency.
```
### 3.11 External interrupt/event controller (EXTI)

```
The external interrupt/event controller consists of 21 edge-detector lines used to generate
interrupt/event requests. Each line can be independently configured to select the trigger
event (rising edge, falling edge, both) and can be masked independently. A pending register
maintains the status of the interrupt requests. The EXTI can detect an external line with a
pulse width shorter than the Internal APB2 clock period. Up to 81 GPIOs can be connected
to the 16 external interrupt lines.
```
### 3.12 Clocks and startup

```
On reset the 16 MHz internal RC oscillator is selected as the default CPU clock. The
16 MHz internal RC oscillator is factory-trimmed to offer 1% accuracy at 25 °C. The
application can then select as system clock either the RC oscillator or an external 4-26 MHz
clock source. This clock can be monitored for failure. If a failure is detected, the system
automatically switches back to the internal RC oscillator and a software interrupt is
generated (if enabled). This clock source is input to a PLL thus allowing to increase the
frequency up to 100 MHz. Similarly, full interrupt management of the PLL clock entry is
available when necessary (for example if an indirectly used external oscillator fails).
```
```
Several prescalers allow the configuration of the two AHB buses, the high-speed APB
(APB2) and the low-speed APB (APB1) domains. The maximum frequency of the two AHB
```

```
DS10314 Rev 8 21/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
```
buses is 100 MHz while the maximum frequency of the high-speed APB domains is
100 MHz. The maximum allowed frequency of the low-speed APB domain is 50 MHz.
```
```
The devices embed a dedicated PLL (PLLI2S), which allows to achieve audio class
performance. In this case, the I^2 S master clock can generate all standard sampling
frequencies from 8 kHz to 192 kHz.
```
### 3.13 Boot modes

```
At startup, boot pins are used to select one out of three boot options:
```
- Boot from user flash memory
- Boot from system memory
- Boot from embedded SRAM

```
The bootloader is located in system memory. It is used to reprogram the flash memory by
using USART1(PA9/10), USART2(PD5/6), USB OTG FS in device mode (PA11/12) through
DFU (device firmware upgrade), I2C1(PB6/7), I2C2(PB10/3), I2C3(PA8/PB4),
SPI1(PA4/5/6/7), SPI2(PB12/13/14/15) or SPI3(PA15, PC10/11/12).
```
```
For more detailed information on the bootloader, refer to Application Note: AN2606, STM32
microcontroller system memory boot mode.
```
### 3.14 Power supply schemes

- VDD = 1.7 to 3.6 V: external power supply for I/Os with the internal supervisor
    (POR/PDR) disabled, provided externally through VDD pins. Requires the use of an
    external power supply supervisor connected to the VDD and NRST pins.
- VSSA, VDDA = 1.7 to 3.6 V: external analog power supplies for ADC, Reset blocks, RCs,
    and PLL. VDDA and VSSA must be connected to VDD and VSS, respectively, with
    decoupling technique.
- VBAT = 1.65 to 3.6 V: power supply for RTC, external clock 32 kHz oscillator and
    backup registers (through power switch) when VDD is not present.

```
Refer to Figure 17: Power supply scheme for more details.
```

**Functional overview STM32F411xC STM32F411xE**

22/151 DS10314 Rev 8

### 3.15 Power supply supervisor

#### 3.15.1 Internal reset ON

```
This feature is available for VDD operating voltage range 1.8 V to 3.6 V.
```
```
The internal power supply supervisor is enabled by holding PDR_ON high.
```
```
The devices have an integrated power-on reset (POR) / power-down reset (PDR) circuitry
coupled with a brownout reset (BOR) circuitry. At power-on, POR is always active, and
ensures proper operation starting from 1.8 V. After the 1.8 V POR threshold level is
reached, the option byte loading process starts, either to confirm or modify default
thresholds, or to disable BOR permanently. Three BOR thresholds are available through
option bytes.
```
```
The devices remain in reset mode when VDD is below a specified threshold, VPOR/PDR, or
VBOR, without the need for an external reset circuit.
```
```
The devices also feature an embedded programmable voltage detector (PVD) that monitors
the VDD/VDDA power supply and compares it to the VPVD threshold. An interrupt can be
generated when VDD/VDDA drops below the VPVD threshold and/or when VDD/VDDA is
higher than the VPVD threshold. The interrupt service routine can then generate a warning
message and/or put the MCU into a safe state. The PVD is enabled by software.
```
#### 3.15.2 Internal reset OFF

```
This feature is available only on packages featuring the PDR_ON pin. The internal power-on
reset (POR) / power-down reset (PDR) circuitry is disabled by setting the PDR_ON pin to
low.
```
```
An external power supply supervisor should monitor VDD and should set the device in reset
mode when VDD is below 1.7 V. NRST should be connected to this external power supply
supervisor. Refer to Figure 5.
```
## Figure 5. Power supply supervisor interconnection with internal reset OFF

1. The PRD_ON pin is only available on the WLCSP49 and UFBGA100 packages.

```
MSv34975V1
```
```
PDR_ON
```
```
VDD
```
```
NRST
```
```
External VDD power supply supervisor
```
```
Ext. reset controller active when
VDD < 1.7 V
```
```
VDD
```

```
DS10314 Rev 8 23/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
```
A comprehensive set of power-saving mode allows to design low-power applications.
```
```
When the internal reset is OFF, the following integrated features are no longer supported:
```
- The integrated power-on reset (POR) / power-down reset (PDR) circuitry is disabled.
- The brownout reset (BOR) circuitry must be disabled.
- The embedded programmable voltage detector (PVD) is disabled.
- VBAT functionality is no more available and the VBAT pin should be connected to VDD.

### 3.16 Voltage regulator

```
The regulator has four operating modes:
```
- Regulator ON
    - Main regulator mode (MR)
    - Low power regulator (LPR)
    - Power-down
- Regulator OFF

#### 3.16.1 Regulator ON

```
On packages embedding the BYPASS_REG pin, the regulator is enabled by holding
BYPASS_REG low. On all other packages, the regulator is always enabled.
```
```
There are three power modes configured by software when the regulator is ON:
```
- MR is used in the nominal regulation mode (With different voltage scaling in Run)
    In Main regulator mode (MR mode), different voltage scaling are provided to reach the
    best compromise between maximum frequency and dynamic power consumption.
- LPR is used in the Stop modes
    The LP regulator mode is configured by software when entering Stop mode.
- Power-down is used in Standby mode.
    The Power-down mode is activated only when entering in Standby mode. The regulator
    output is in high impedance and the kernel circuitry is powered down, inducing zero
    consumption. The contents of the registers and SRAM are lost.

```
Depending on the package, one or two external ceramic capacitors should be connected on
the VCAP_1 and VCAP_2 pins. The VCAP_2 pin is only available for the LQFP100 and
UFBGA100 packages.
```
```
All packages have the regulator ON feature.
```
#### 3.16.2 Regulator OFF

```
The Regulator OFF is available only on the UFBGA100, which features the BYPASS_REG
pin. The regulator is disabled by holding BYPASS_REG high. The regulator OFF mode
allows to supply externally a V12 voltage source through VCAP_1 and VCAP_2 pins.
```
```
Since the internal voltage scaling is not managed internally, the external voltage value must
be aligned with the targeted maximum frequency. Refer to Table 14: General operating
conditions.
```
```
The two 2.2 μF VCAP ceramic capacitors should be replaced by two 100 nF decoupling
capacitors. Refer to Figure 17: Power supply scheme.
```

**Functional overview STM32F411xC STM32F411xE**

24/151 DS10314 Rev 8

```
When the regulator is OFF, there is no more internal monitoring on V12. An external power
supply supervisor should be used to monitor the V12 of the logic power domain. PA0 pin
should be used for this purpose, and act as a power-on reset on the V12 power domain.
```
```
In regulator OFF mode, the following features are no more supported:
```
- PA0 cannot be used as a GPIO pin since it allows to reset a part of the V12 logic power
    domain, which is not reset by the NRST pin.
- As long as PA0 is kept low, the debug mode cannot be used under power-on reset. As
    a consequence, PA0 and NRST pins must be managed separately if the debug
    connection under reset or prereset is required.

## Figure 6. Regulator OFF

```
The following conditions must be respected:
```
- VDD should always be higher than VCAP_1 and VCAP_2 to avoid current injection
    between power domains.
- If the time for VCAP_1 and VCAP_2 to reach V 12 minimum value is faster than the time for
    VDD to reach 1.7 V, then PA0 should be kept low to cover both conditions: until VCAP_1
    and VCAP_2 reach V 12 minimum value and until VDD reaches 1.7 V (see _Figure 7_ ).
- Otherwise, if the time for VCAP_1 and VCAP_2 to reach the V 12 minimum value is slower
    than the time for VDD to reach 1.7 V, then PA0 could be asserted low externally (see
    _Figure 8_ ).
- If VCAP_1 and VCAP_2 go below V 12 minimum value and VDD is higher than 1.7 V, then a
    reset must be asserted on PA0 pin.

_Note: The minimum value of V 12 depends on the maximum frequency targeted in the application_

```
ai18498V3
```
```
BYPASS_REG
```
```
VCAP_1
```
```
VCAP_2
```
```
PA0
```
```
V12
```
```
VDD
NRST
VDD
```
```
Application reset
signal (optional)
```
```
External VCAP_1/2 power
supply supervisor
Ext. reset controller active
when VCAP_1/2 < Min V 12
```
```
V12
```

```
DS10314 Rev 8 25/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
```
Figure 7. Startup in regulator OFF: slow VDD slope -
power-down reset risen after VCAP_1/VCAP_2 stabilization
```
1. This figure is valid whatever the internal reset mode (ON or OFF).

```
Figure 8. Startup in regulator OFF mode: fast VDD slope -
power-down reset risen before VCAP_1/VCAP_2 stabilization
```
1. This figure is valid whatever the internal reset mode (ON or OFF).

```
MSv31179V1
```
```
VDD
```
```
time
```
```
Min V 12
```
```
PDR = 1.7 V VCAP_1/VCAP_2
V 12
```
```
NRST
```
```
time
```
```
VDD
```
```
time
```
```
Min V 12
```
```
VCAP_1/VCAP_2
V 12
```
```
PA0 asserted externally
```
```
NRST
```
```
time MSv31180V1
```
```
PDR = 1.7 V
```

**Functional overview STM32F411xC STM32F411xE**

26/151 DS10314 Rev 8

#### 3.16.3 Regulator ON/OFF and internal power supply supervisor availability

### 3.17 Real-time clock (RTC) and backup registers

```
The backup domain includes:
```
- The real-time clock (RTC)
- 20 backup registers

```
The real-time clock (RTC) is an independent BCD timer/counter. Dedicated registers contain
the second, minute, hour (in 12/24 hour), week day, date, month, year, in BCD (binary-
coded decimal) format. Correction for 28th, 29th (leap year), 30th, and 31st day of the month
are performed automatically. The RTC features a reference clock detection, a more precise
second source clock (50 or 60 Hz) can be used to enhance the calendar precision. The RTC
provides a programmable alarm and programmable periodic interrupts with wakeup from
Stop and Standby modes. The subseconds value is also available in binary format.
```
```
It is clocked by a 32.768 kHz external crystal, resonator or oscillator, the internal low-power
RC oscillator or the high-speed external clock divided by 128. The internal low-speed RC
has a typical frequency of 32 kHz. The RTC can be calibrated using an external 512 Hz
output to compensate for any natural quartz deviation.
```
```
Two alarm registers are used to generate an alarm at a specific time and calendar fields can
be independently masked for alarm comparison. To generate a periodic interrupt, a 16-bit
programmable binary autoreload downcounter with programmable resolution is available
and allows automatic wakeup and periodic alarms from every 120 μs to every 36 hours.
```
```
A 20-bit prescaler is used for the time base clock. It is by default configured to generate a
time base of 1 second from a clock at 32.768 kHz.
```
```
The backup registers are 32-bit registers used to store 80 bytes of user application data
when VDD power is not present. Backup registers are not reset by a system, a power reset,
or when the device wakes up from the Standby mode (see Section 3.18 ).
```
```
Additional 32-bit registers contain the programmable alarm subseconds, seconds, minutes,
hours, day, and date.
```
```
The RTC and backup registers are supplied through a switch that is powered either from the
VDD supply when present or from the VBAT pin.
```
## Table 3. Regulator ON/OFF and internal power supply supervisor availability.

```
Package Regulator ON Regulator OFF
```
```
Power supply
supervisor ON
```
```
Power supply
supervisor OFF
```
```
UFQFPN48 Yes No Yes No
```
```
WLCSP49 Yes No Ye s
PDR_ON set to VDD
```
```
Ye s
PDR_ON external
control(1)
```
```
LQFP64 Yes No Yes No
```
```
LQFP100 Yes No Yes No
```
##### UFBGA100

```
Ye s
BYPASS_REG set to
VSS
```
```
Ye s
BYPASS_REG set to
VDD
```
```
Ye s
PDR_ON set to VDD
```
```
Ye s
PDR_ON external
control (1)
```
1. Refer to _Section 3.15: Power supply supervisor_


```
DS10314 Rev 8 27/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
### 3.18 Low-power modes

```
The devices support three low-power modes to achieve the best compromise between low
power consumption, short startup time and available wakeup sources:
```
- **Sleep mode**
    In Sleep mode, only the CPU is stopped. All peripherals continue to operate and can
    wake up the CPU when an interrupt/event occurs.
    To further reduce the power consumption, the flash memory can be switched off before
    entering in Sleep mode. Note that this requires a code execution from the RAM.
- **Stop mode**
    The Stop mode achieves the lowest power consumption while retaining the contents of
    SRAM and registers. All clocks in the 1.2 V domain are stopped, the PLL, the HSI RC
    and the HSE crystal oscillators are disabled. The voltage regulator can also be put
    either in normal or in low-power mode.
    The devices can be woken up from the Stop mode by any of the EXTI lines (the EXTI
    line source can be one of the 16 external lines, the PVD output, the RTC alarm/
    wakeup/ tamper/ time stamp events).
- **Standby mode**
    The Standby mode is used to achieve the lowest power consumption. The internal
    voltage regulator is switched off so that the entire 1.2 V domain is powered off. The
    PLL, the HSI RC and the HSE crystal oscillators are also switched off. After entering
    Standby mode, the SRAM and register contents are lost except for registers in the
    backup domain when selected.
    The devices exit the Standby mode when an external reset (NRST pin), an IWDG reset,
    a rising edge on the WKUP pin, or an RTC alarm/ wakeup/ tamper/time stamp event
    occurs.
    Standby mode is not supported when the embedded voltage regulator is bypassed and
    the 1.2 V domain is controlled by an external power.

### 3.19 VBAT operation

```
The VBAT pin allows to power the device VBAT domain from an external battery, an external
super-capacitor, or from VDD when no external battery and an external super-capacitor are
present.
```
```
VBAT operation is activated when VDD is not present.
```
```
The VBAT pin supplies the RTC and the backup registers.
```
_Note: When the microcontroller is supplied from VBAT, external interrupts and RTC alarm/events
do not exit it from VBAT operation. When PDR_ON pin is not connected to VDD (internal
Reset OFF), the VBAT functionality is no more available and VBAT pin should be connected
to VDD._


**Functional overview STM32F411xC STM32F411xE**

28/151 DS10314 Rev 8

### 3.20 Timers and watchdogs

```
The devices embed one advanced-control timer, seven general-purpose timers, and two
watchdog timers.
```
```
All timer counters can be frozen in debug mode.
```
```
Ta b l e 4 compares the features of the advanced-control and general-purpose timers.
```
#### 3.20.1 Advanced-control timers (TIM1)

```
The advanced-control timer (TIM1) can be seen as three-phase PWM generators
multiplexed on four independent channels. It has complementary PWM outputs with
programmable inserted dead times. It can also be considered as a complete general-
purpose timer. Its four independent channels can be used for:
```
- Input capture
- Output compare
- PWM generation (edge- or center-aligned modes)
- One-pulse mode output

## Table 4. Timer feature comparison

```
Timer
type Timer
```
```
Counter
resolution
```
```
Counter
type
```
```
Prescaler
factor
```
##### DMA

```
request
generation
```
```
Capture/
compare
channels
```
```
Complemen-
tary output
```
```
Max.
interface
clock
(MHz)
```
```
Max.
timer
clock
(MHz)
```
```
Advanced
-control
```
```
TIM1 16-bit
```
```
Up,
Down,
Up/down
```
```
Any
integer
between 1
and
65536
```
```
Yes 4 Yes 100 100
```
```
General
purpose
```
##### TIM2,

##### TIM5

```
32-bit
```
```
Up,
Down,
Up/down
```
```
Any
integer
between 1
and
65536
```
```
Yes 4 No 50 100
```
##### TIM3,

##### TIM4

```
16-bit
```
```
Up,
Down,
Up/down
```
```
Any
integer
between 1
and
65536
```
```
Yes 4 No 50 100
```
```
TIM9 16-bit Up
```
```
Any
integer
between 1
and
65536
```
```
No 2 No 100 100
```
##### TIM10,

##### TIM11

```
16-bit Up
```
```
Any
integer
between 1
and
65536
```
```
No 1 No 100 100
```

```
DS10314 Rev 8 29/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
```
If configured as a standard 16-bit timers, it has the same features as the general-purpose
TIMx timers. If configured as a 16-bit PWM generator, it has full modulation capability
(0-100%).
```
```
The advanced-control timer can work together with the TIMx timers via the Timer Link
feature for synchronization or event chaining.
```
```
TIM1 supports independent DMA request generation.
```
#### 3.20.2 General-purpose timers (TIMx)

```
There are seven synchronizable general-purpose timers embedded in the
STM32F411xC/xE (see Table 4 for differences).
```
- **TIM2, TIM3, TIM4, TIM5**
    The STM32F411xC/xE devices are 4 full-featured general-purpose timers: TIM2, TIM5,
    TIM3, and TIM4.The TIM2 and TIM5 timers are based on a 32-bit autoreload
    up/downcounter and a 16-bit prescaler. The TIM3 and TIM4 timers are based on a 16-
    bit autoreload up/downcounter and a 16-bit prescaler. They all feature four independent
    channels for input capture/output compare, PWM or one-pulse mode output. This gives
    up to 15 input capture/output compare/PWMs.
    The TIM2, TIM3, TIM4, TIM5 general-purpose timers can work together, or with the
    other general-purpose timers and the advanced-control timer TIM1 via the timer Link
    feature for synchronization or event chaining.
    Any of these general-purpose timers can be used to generate PWM outputs.
    TIM2, TIM3, TIM4, TIM5 all have independent DMA request generation. They are
    capable of handling quadrature (incremental) encoder signals and the digital outputs
    from 1 to 4 hall-effect sensors.
- **TIM9, TIM10, and TIM11**
    These timers are based on a 16-bit autoreload upcounter and a 16-bit prescaler. TIM10
    and TIM11 feature one independent channel, whereas TIM9 has two independent
    channels for input capture/output compare, PWM or one-pulse mode output. They can
    be synchronized with the TIM2, TIM3, TIM4, TIM5 full-featured general-purpose timers.
    They can also be used as simple time bases.

#### 3.20.3 Independent watchdog

```
The independent watchdog is based on a 12-bit downcounter and 8-bit prescaler. It is
clocked from an independent 32 kHz internal RC and as it operates independently from the
main clock, it can operate in Stop and Standby modes. It can be used either as a watchdog
to reset the device when a problem occurs, or as a free-running timer for application timeout
management. It is hardware- or software-configurable through the option bytes.
```
#### 3.20.4 Window watchdog

```
The window watchdog is based on a 7-bit downcounter that can be set as free-running. It
can be used as a watchdog to reset the device when a problem occurs. It is clocked from
the main clock. It has an early warning interrupt capability and the counter can be frozen in
debug mode.
```

**Functional overview STM32F411xC STM32F411xE**

30/151 DS10314 Rev 8

#### 3.20.5 SysTick timer

```
This timer is dedicated to real-time operating systems, but could also be used as a standard
downcounter. It features:
```
- A 24-bit downcounter
- Auto reload capability
- Maskable system interrupt generation when the counter reaches 0
- Programmable clock source.

### 3.21 Inter-integrated circuit interface (I2C)

```
Up to three I^2 C bus interfaces can operate in multimaster and slave modes. They can
support the standard (up to 100 kHz) and fast (up to 400 kHz) modes. The I2C bus
frequency can be increased up to 1 MHz. For more details about the complete solution,
please contact your local ST sales representative. They also support the 7/10-bit addressing
mode and the 7-bit dual addressing mode (as slave). A hardware CRC
generation/verification is embedded.
```
```
They can be served by DMA and they support SMBus 2.0/PMBus.
```
```
The devices also include programmable analog and digital noise filters (see Ta b l e 5 ).
```
### 3.22 Universal synchronous/asynchronous receiver transmitters (USART)

```
The devices embed three universal synchronous/asynchronous receiver transmitters
(USART1, USART2, and USART6).
```
```
These three interfaces provide asynchronous communication, IrDA SIR ENDEC support,
multiprocessor communication mode, single-wire half-duplex communication mode and
have LIN Master/Slave capability. The USART1 and USART6 interfaces are able to
communicate at speeds of up to 12.5 Mbit/s. The USART2 interface communicates at up to
6.25 bit/s.
```
```
USART1 and USART2 also provide hardware management of the CTS and RTS signals,
smartcard mode (ISO 7816 compliant) and SPI-like communication capability. All interfaces
can be served by the DMA controller.
```
## Table 5. Comparison of I2C analog and digital filters

**- Analog filter Digital filter**

```
Pulse width of
suppressed spikes ≥ 50 ns Programmable length from 1 to 15 I2C peripheral clocks
```

```
DS10314 Rev 8 31/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
### 3.23 Serial peripheral interface (SPI)

```
The devices feature five SPIs in slave and master modes in full-duplex and simplex
communication modes. SPI1, SPI4, and SPI5 can communicate at up to 50 Mbit/s, SPI2
and SPI3 can communicate at up to 25 Mbit/s. The 3-bit prescaler gives 8 master mode
frequencies and the frame is configurable to 8 bits or 16 bits. The hardware CRC
generation/verification supports basic SD Card/MMC modes. All SPIs can be served by the
DMA controller.
```
```
The SPI interface can be configured to operate in TI mode for communications in master
mode and slave mode.
```
### 3.24 Inter-integrated sound (I^2 S)

```
Five standard I^2 S interfaces (multiplexed with SPI1 to SPI5) are available. They can be
operated in master or slave mode, in simplex communication modes and full duplex for I2S2
and I2S3 and can be configured to operate with a 16-/32-bit resolution as an input or output
channel. All the I2Sx audio sampling frequencies from 8 kHz up to 192 kHz are supported.
When either or both of the I^2 S interfaces is/are configured in master mode, the master clock
can be output to the external DAC/CODEC at 256 times the sampling frequency.
```
```
All I^2 Sx can be served by the DMA controller.
```
### 3.25 Audio PLL (PLLI2S)

```
The devices feature an additional dedicated PLL for audio I^2 S application, making it
possible to achieve error-free I^2 S sampling clock accuracy without compromising on the
CPU performance.
```
```
The PLLI2S configuration can be modified to manage an I^2 S sample rate change without
disabling the main PLL (PLL) used for the CPU.
```
```
The audio PLL can be programmed with very low error to obtain sampling rates ranging
from 8 kHz to 192 kHz.
```
## Table 6. USART feature comparison

##### USART

```
name
```
```
Standard
features
```
```
Modem
(RTS/CTS) LIN
```
##### SPI

```
master irDA
```
```
Smartcard
(ISO 7816)
```
```
Max. baud
rate in Mbit/s
(oversampling
by 16)
```
```
Max. baud
rate in Mbit/s
(oversampling
by 8)
```
##### APB

```
mapping
```
##### USART1 X X X X X X 6.25 12.5

##### APB2

```
(max.
100 MHz)
```
##### USART2 X X X X X X 3.12 6.25

##### APB1

```
(max.
50 MHz)
```
##### USART6 X N.A X X X X 6.25 12.5

##### APB2

```
(max.
100 MHz)
```

**Functional overview STM32F411xC STM32F411xE**

32/151 DS10314 Rev 8

```
In addition to the audio PLL, a master clock input pin can be used to synchronize the I2S
flow with an external PLL (or Codec output).
```
### 3.26 Secure digital input/output interface (SDIO)

```
An SD/SDIO/MMC/eMMC host interface is available that supports MultiMediaCard System
Specification Version 4.2 in three different databus modes: 1-bit (default), 4-bit and 8-bit.
```
```
The interface allows data transfer at up to 50 MHz, and is compliant with the SD memory
card specification version 2.0.
```
```
The SDIO Card Specification Version 2.0 is also supported with two different databus
modes: 1-bit (default) and 4-bit.
```
```
The current version supports only one SD/SDIO/MMC4.2 card at any one time and a stack
of MMC4.1 or previous.
```
```
In addition to SD/SDIO/MMC/eMMC, this interface is fully compliant with the CE-ATA digital
protocol Rev1.1.
```
### 3.27 Universal serial bus on-the-go full-speed (OTG_FS)

```
The devices embed a USB OTG full-speed device/host/OTG peripheral with integrated
transceivers. The USB OTG FS peripheral is compliant with the USB 2.0 specification and
with the OTG 1.0 specification. It has software-configurable endpoint setting and supports
suspend/resume. The USB OTG full-speed controller requires a dedicated 48 MHz clock
that is generated by a PLL connected to the HSE oscillator. The major features are:
```
- Combined Rx and Tx FIFO size of 320 × 35 bits with dynamic FIFO sizing
- Supports the session request protocol (SRP) and host negotiation protocol (HNP)
- 4 bidirectional endpoints
- 8 host channels with periodic OUT support
- HNP/SNP/IP inside (no need for any external resistor)
- For OTG/Host modes, a power switch is needed in case bus-powered devices are
    connected

### 3.28 General-purpose input/outputs (GPIOs)

```
Each of the GPIO pins can be configured by software as output (push-pull or open-drain,
with or without pull-up or pull-down), as input (floating, with or without pull-up or pull-down)
or as peripheral alternate function. Most of the GPIO pins are shared with digital or analog
alternate functions. All GPIOs are high-current-capable and have speed selection to better
manage internal noise, power consumption and electromagnetic emission.
```
```
The I/O configuration can be locked if needed by following a specific sequence to avoid
spurious writing to the I/Os registers.
```
```
Fast I/O handling allowing maximum I/O toggling up to 100 MHz.
```

```
DS10314 Rev 8 33/151
```
**STM32F411xC STM32F411xE Functional overview**

```
57
```
### 3.29 Analog-to-digital converter (ADC)

```
One 12-bit analog-to-digital converter is embedded and shares up to 16 external channels,
performing conversions in the single-shot or scan mode. In scan mode, automatic
conversion is performed on a selected group of analog inputs.
```
```
The ADC can be served by the DMA controller. An analog watchdog feature allows very
precise monitoring of the converted voltage of one, some or all selected channels. An
interrupt is generated when the converted voltage is outside the programmed thresholds.
```
```
To synchronize A/D conversion and timers, the ADCs could be triggered by any of the TIM1,
TIM2, TIM3, TIM4, or TIM5 timer.
```
### 3.30 Temperature sensor

```
The temperature sensor has to generate a voltage that varies linearly with temperature. The
conversion range is between 1.7 V and 3.6 V. The temperature sensor is internally
connected to the ADC_IN18 input channel, which is used to convert the sensor output
voltage into a digital value. Refer to the reference manual for additional information.
```
```
As the offset of the temperature sensor varies from chip to chip due to process variation, the
internal temperature sensor is mainly suitable for applications that detect temperature
changes instead of absolute temperatures. If an accurate temperature reading is needed,
then an external temperature sensor part should be used.
```
### 3.31 Serial wire JTAG debug port (SWJ-DP)

```
The Arm SWJ-DP interface is embedded, and is a combined JTAG and serial wire debug
port that enables either a serial wire debug or a JTAG probe to be connected to the target.
```
```
Debug is performed using 2 pins only instead of the five reuse as GPIO with the alternate
function): the JTAG TMS and TCK pins are shared with SWDIO and SWCLK, respectively,
and a specific sequence on the TMS pin is used to switch between JTAG-DP and SW-DP.
```
### 3.32 Embedded Trace Macrocell™

```
The Arm Embedded Trace Macrocell provides a greater visibility of the instruction and data
flow inside the CPU core by streaming compressed data at a very high rate from the
STM32F411xC/xE through a small number of ETM pins to an external hardware trace port
analyzer (TPA) device. The TPA is connected to a host computer using any high-speed
channel available. Real-time instruction and data flow activity can be recorded and then
formatted for display on the host computer that runs the debugger software. TPA hardware
is commercially available from common development tool vendors.
```
```
The Embedded Trace Macrocell operates with third party debugger software tools.
```

**Pinouts and pin description STM32F411xC STM32F411xE**

34/151 DS10314 Rev 8

## 4 Pinouts and pin description

## Figure 9. STM32F411xC/xE WLCSP49 pinout

1. The above figure shows the package bump side.

```
MS34976V1
```
A

B

E

D

C

F

G

```
VDD
```
```
PC14-
OSC32_IN
```
```
VBAT
```
```
PH0-
OSC_IN
```
```
NRST
```
```
VDDA
VREF+
```
```
PA1
```
```
VSS
```
```
PDR
_ON
```
```
PC15-
OSC32_OUT
```
```
VSSA
VREF-
```
```
PA0
```
```
PA4
```
```
BOOT0
```
```
PB8
```
```
PH1-
OSC_OUT
```
```
PB0
```
```
PB4
```
```
PB5
```
```
PB6
```
```
PB3
```
```
PA13
```
```
PA15
```
```
VDD
```
```
PA14
```
```
VSS
```
```
PA12
```
```
PB 2
```
```
PA9
```
```
PB12
```
```
PA10
```
```
VCAP1
```
```
PA 11
```
```
VDD PB14
```
```
VSS
```
```
PB10
```
```
PA7
```
```
PA 8
```
```
PB15
```
```
PB13
```
7 6 5 4 3 2 1

```
PB9
```
```
PB1
```
```
PC13
```
```
PA2
```
```
PA5
```
```
PA3
```
```
PA6
```
```
PB7
```

```
DS10314 Rev 8 35/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
## Figure 10. STM32F411xC/xE UFQFPN48 pinout

1. The above figure shows the package top view.

```
MS31150V5
```
```
VSS BOOT0PB7PB6PB5PB4PB3PA15PA14
```
```
48 47 46 45 44 43 42 41 40
1 36 VDD
2 35 VSS
3 34 PA13
4
```
```
UFQFPN48
```
```
33 PA12
```
```
VSSA/VREF-
```
```
5 32 PA11
```
```
VDDA/VREF+
```
```
6 31 PA10
```
```
PA0
```
```
7 30 PA9
```
```
PA1
```
```
8 29 PA8
```
```
PA2
```
(^928)
VDD
13 14 15 16 17 18 19 20 21
PA3PA4PA5PA6PA7PB0PB1PB2 VSS
10
11
12
27
26
25
22 23 24
39 38 37
PB10
VCAP_1
PB15
PB14
PB13
PB12
VBAT
PC13
PC14-OSC32_IN
PH0-OSC_IN
NRST
VDD PB9 PB8
PC15-OSC32_OUT
PH1-OSC_OUT


**Pinouts and pin description STM32F411xC STM32F411xE**

36/151 DS10314 Rev 8

## Figure 11. STM32F411xC/xE LQFP64 pinout

1. The above figure shows the package top view.

```
64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49
48
47
46
45
44
43
42
41
40
39
38
37
36
35
34
33
17 18 19 20 21 22 23 2425 26 27 2829 30 31 32
```
```
1 2 3 4 5 6 7 8 9
```
```
10
11
12
13
14
15
16
```
```
VBAT
```
```
PC14-OSC32_IN
```
```
PH0-OSC_IN
```
```
NRST
PC0
PC1
PC2
PC3
VSSA/VREF-
VDDA/VREF+
PA0
PA1
PA2
```
```
VDD PB9PB8BOOT0PB7PB6PB5PB4PB3PD2 PC12PC11PC10PA15PA14
```
```
VDD
VSS
PA13
PA12
PA11
PA10
PA9
PA8
PC9
PC8
PC7
PC6
PB15
PB14
PB13
PB12
```
```
PA3VSS PA4PA5PA6PA7PC4PC5PB0PB1PB2PB10
VCAP_1
```
```
LQFP64
```
```
PC13
```
```
MS31149V3
```
```
VDD
```
```
VSS
```
```
VSSVDD
```
```
PH1-OSC_OUT
```
```
PC15-OSC32_OUT
```

```
DS10314 Rev 8 37/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
## Figure 12. STM32F411xC/xE LQFP100 pinout

1. The above figure shows the package top view.

```
100999897969594939291908988878685848382818079787776
1 2 3 4 5 6 7 8 9
```
```
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
```
```
75
74
73
72
71
70
69
68
67
66
65
64
63
62
61
60
59
58
57
56
55
54
53
52
51
```
```
PE2
PE3
PE4
PE5
PE6
VBAT
```
```
PC14-OSC32_IN
PC15-OSC32_OUT
VSS
VDD
PH0-OSC_IN
```
```
NRST
PC0
PC1
PC2
PC3
VDD
VSSA/VREF-
```
```
VDDA
```
```
VREF+
```
```
PA0
PA1
PA2
```
```
VDD
VSS
VCAP_2
PA13
PA12
PA11
PA10
PA9
PA8
PC9
PC8
PC7
PC6
PD15
PD14
PD13
PD12
PD11
PD10
PD9
PD8
PB15
PB14
PB13
PB12
```
```
PA3VSSVDDPA4PA5PA6PA7PC4PC5PB0PB1PB2PE7PE8PE9PE10PE11PE12PE13PE14PE15PB10
VCAP_1
```
```
VSSVDD
```
```
VDDVSSPE1 PE0 PB9 PB8 BOOT0 PB7 PB6 PB5 PB4 PB3 PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0 PC12 PC11 PC10 PA15 PA14
```
```
26272829303132333435363738394041424344454647484950
```
```
MS31151V4
```
##### LQFP100

```
PC13
```
```
PH1-OSC_OUT
```

**Pinouts and pin description STM32F411xC STM32F411xE**

38/151 DS10314 Rev 8

## Figure 13. STM32F411xC/xE UFBGA100 pinout

1. This figure shows the package top view

```
069
```
```
$
```
```
%
```
```
(
```
```
'
```
```
& ) * + -. / 0
```
```
3(
```
```
26&B,1
```
```
3&
26&B287
```
```
3&
```
```
3&
$17,B7$03
```
```
3(
```
```
26&B287
```
```
3&
```
```
966$
```
```
95()
```
```
95()
```
```
9''$
```
```
3(
```
```
3(
```
```
3(
```
```
3(
```
```
9%$7
```
```
966
```
```
9''
```
```
1567
```
```
3&
```
```
3&
```
```
3$
:.83
```
```
3$
```
```
3%
```
```
3(
```
```
3%
```
```
966
```
```
%<3$66B5(*
```
```
3'5B21
```
```
3&
```
```
3$
```
```
3$
```
```
3$
```
```
%227
```
```
3%
```
```
9''
```
```
3$
```
```
3$
```
```
3$
```
```
3'
```
```
3%
```
```
3%
```
```
3&
```
```
3&
```
```
3%
```
```
3'
```
```
3'
```
```
3%
```
```
3%
```
```
3%
```
```
3'
```
```
3(
```
```
3(
```
```
3%
```
```
3'
```
```
3'
```
```
3'
```
```
3(
```
```
3(
```
```
3$
```
```
3'
```
```
3'
```
```
3%
```
```
3(
```
```
3(
```
```
3$
```
```
3&
```
```
3&
```
```
3&
```
```
3$
```
```
3'
```
```
3'
```
```
3%
```
```
3%
```
```
3(
```
```
3$
```
```
3&
```
```
9&$3
B
```
```
3$
```
```
3&
```
```
3'
```
```
3'
```
```
3%
```
```
9&$3
B
```
```
3(
```
```
966
```
```
9''
```
```
3$
```
```
3$
```
```
3$
```
```
3&
```
```
3&
```
```
3'
```
```
3'
```
```
3%
```
```
3%
```
```
3(
```
```
966
```
```
9''
```
```
           
```
```
3+
```
```
3+
```
```
26&B,1
```

```
DS10314 Rev 8 39/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
## Table 7. Legend/abbreviations used in the pinout table

```
Name Abbreviation Definition
```
```
Pin name
```
```
Unless otherwise specified in brackets below the pin name, the pin function during and after
reset is the same as the actual pin name
```
```
Pin type
```
```
S Supply pin
```
```
I Input only pin
```
```
I/O Input/ output pin
```
```
I/O structure
```
```
FT 5 V tolerant I/O
```
```
TC Standard 3.3 V I/O
```
```
B Dedicated BOOT0 pin
```
```
NRST Bidirectional reset pin with embedded weak pull-up resistor
```
```
Notes Unless otherwise specified by a note, all I/Os are set as floating inputs during and after reset
```
```
Alternate
functions
```
```
Functions selected through GPIOx_AFR registers
```
```
Additional
functions Functions directly selected/enabled through peripheral registers
```
## Table 8. STM32F411xC/xE pin definitions

```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100

##### - - - 1 B2 PE2 I/O FT -

##### TRACECLK,

##### SPI4_SCK/I2S4_CK,

##### SPI5_SCK/I2S5_CK,

##### EVENTOUT

##### -

##### - - - 2 A1 PE3 I/O FT -

##### TRACED0,

##### EVENTOUT -

##### - - - 3 B1 PE4 I/O FT -

##### TRACED1,

##### SPI4_NSS/I2S4_WS,

##### SPI5_NSS/I2S5_WS,

##### EVENTOUT

##### -

##### - - - 4 C2 PE5 I/O FT -

##### TRACED2,

##### TIM9_CH1,

##### SPI4_MISO,

##### SPI5_MISO,

##### EVENTOUT

##### -


**Pinouts and pin description STM32F411xC STM32F411xE**

40/151 DS10314 Rev 8

##### - - - 5 D2 PE6 I/O FT -

##### TRACED3,

##### TIM9_CH2,

##### SPI4_MOSI/I2S4_SD,

##### SPI5_MOSI/I2S5_SD,

##### EVENTOUT

##### -

##### ----D3VSS S-- - -

##### ----C4VDD S-- - -

##### 1 1 B7 6 E2 VBAT S - - - -

##### 22D57C1

##### PC13-

##### ANTI_TAMP I/O FT

##### (2)(3) - RTC_AMP1,

##### RTC_OUT, RTC_TS

##### 33C78D1

##### PC14-

##### OSC32_IN I/O FT

```
(2)(3)
(4) - OSC32_IN
```
##### 44C69E1

##### PC15-

##### OSC32_OUT I/O FT - - OSC32_OUT

##### - - - 10 F2 VSS S - - - -

##### ---11G2VDD S-- - -

##### 5 5 D7 12 F1 PH0 - OSC_IN I/O FT - - OSC_IN

##### 66D613G1

##### PH1 -

##### OSC_OUT I/O FT - - OSC_OUT

##### 7 7 E7 14 H2 NRST I/O FT - EVENTOUT -

##### - 8 - 15 H1 PC0 I/O FT - EVENTOUT ADC1_10

##### - 9 - 16 J2 PC1 I/O FT - EVENTOUT ADC1_11

##### - 10 - 17 J3 PC2 I/O FT -

##### SPI2_MISO,

```
I2S2ext_SD,
EVENTOUT
```
##### ADC1_12

##### - 11 - 18 K2 PC3 I/O FT -

##### SPI2_MOSI/I2S2_SD,

##### EVENTOUT ADC1_13

##### ---19-VDD S-- - -

##### 8 12 E6 20 - VSSA/VREF- S - - - -

##### - - - - J1 VSSA S - - - -

##### - - - - K1 VREF- S - - - -

##### 9 13 F7 - - VDDA/VREF+ S - - - -

##### ---21L1VREF+ S - -

##### - - - 22 M1 VDDA S - - - -

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


```
DS10314 Rev 8 41/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
##### 10 14 F6 23 L2 PA0-WKUP I/O TC (5)

##### TIM2_CH1/TIM2_ET,

##### TIM5_CH1,

##### USART2_CTS,

##### EVENTOUT

##### ADC1_0, WKUP1

##### 11 15 G7 24 M2 PA1 I/O FT -

##### TIM2_CH2,

##### TIM5_CH2,

##### SPI4_MOSI/I2S4_SD,

##### USART2_RTS,

##### EVENTOUT

##### ADC1_1

##### 12 16 E5 25 K3 PA2 I/O FT -

##### TIM2_CH3,

##### TIM5_CH3,

##### TIM9_CH1,

##### I2S2_CKIN,

##### USART2_TX,

##### EVENTOUT

##### ADC1_2

##### 13 17 E4 26 L3 PA3 I/O FT -

##### TIM2_CH4,

##### TIM5_CH4,

##### TIM9_CH2,

##### I2S2_MCK,

##### USART2_RX,

##### EVENTOUT

##### ADC1_3

##### - 18 - 27 - VSS S - - - -

##### - - - - E3 BYPASS_REG S - - - -

##### - 19 - 28 - VDD I FT - EVENTOUT -

##### 14 20 G6 29 M3 PA4 I/O FT -

##### SPI1_NSS/I2S1_WS,

##### SPI3_NSS/I2S3_WS,

##### USART2_CK,

##### EVENTOUT

##### ADC1_4

##### 15 21 F5 30 K4 PA5 I/O FT -

##### TIM2_CH1/TIM2_ET,

##### SPI1_SCK/I2S1_CK,

##### EVENTOUT

##### ADC1_5

##### 16 22 F4 31 L4 PA6 I/O FT -

##### TIM1_BKIN,

##### TIM3_CH1,

##### SPI1_MISO,

##### I2S2_MCK,

##### SDIO_CMD,

##### EVENTOUT

##### ADC1_6

##### 17 23 F3 32 M4 PA7 I/O FT -

##### TIM1_CH1N,

##### TIM3_CH2,

##### SPI1_MOSI/I2S1_SD,

##### EVENTOUT

##### ADC1_7

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


**Pinouts and pin description STM32F411xC STM32F411xE**

42/151 DS10314 Rev 8

##### - 24 - 33 K5 PC4 I/O FT - EVENTOUT ADC1_14

##### - 25 - 34 L5 PC5 I/O FT - EVENTOUT ADC1_15

##### 18 26 G5 35 M5 PB0 I/O FT -

##### TIM1_CH2N,

##### TIM3_CH3,

##### SPI5_SCK/I2S5_CK,

##### EVENTOUT

##### ADC1_8

##### 19 27 G4 36 M6 PB1 I/O FT -

##### TIM1_CH3N,

##### TIM3_CH4,

##### SPI5_NSS/I2S5_WS,

##### EVENTOUT

##### ADC1_9

##### 20 28 G3 37 L6 PB2 I/O FT - EVENTOUT BOOT1

##### - - - 38 M7 PE7 I/O FT -

##### TIM1_ETR,

##### EVENTOUT -

##### - - - 39 L7 PE8 I/O FT -

##### TIM1_CH1N,

##### EVENTOUT -

##### - - - 40 M8 PE9 I/O FT -

##### TIM1_CH1,

##### EVENTOUT -

##### - - - 41 L8 PE10 I/O FT -

##### TIM1_CH2N,

##### EVENTOUT -

##### - - - 42 M9 PE11 I/O FT -

##### TIM1_CH2,

##### SPI4_NSS/I2S4_WS,

##### SPI5_NSS/I2S5_WS,

##### EVENTOUT

##### -

##### - - - 43 L9 PE12 I/O FT -

##### TIM1_CH3N,

##### SPI4_SCK/I2S4_CK,

##### SPI5_SCK/I2S5_CK,

##### EVENTOUT

##### -

##### - - - 44 M10 PE13 I/O FT -

##### TIM1_CH3,

##### SPI4_MISO,

##### SPI5_MISO,

##### EVENTOUT

##### -

##### - - - 45 M11 PE14 I/O FT -

##### TIM1_CH4,

##### SPI4_MOSI/I2S4_SD,

##### SPI5_MOSI/I2S5_SD,

##### EVENTOUT

##### -

##### - - - 46 M12 PE15 I/O FT -

##### TIM1_BKIN,

##### EVENTOUT -

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


```
DS10314 Rev 8 43/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
##### 21 29 E3 47 L10 PB10 I/O FT -

##### TIM2_CH3,

##### I2C2_SCL,

##### SPI2_SCK/I2S2_CK,

##### I2S3_MCK, SDIO_D7,

##### EVENTOUT

##### -

##### - - - - K9 PB11 I/O FT -

##### TIM2_CH4,

##### I2C2_SDA,

##### I2S2_CKIN,

##### EVENTOUT

##### -

##### 22 30 G2 48 L11 VCAP_1 S - - - -

##### 23 31 D3 49 F12 VSS S - - - -

##### 24 32 F2 50 G12 VDD S - - - -

##### 25 33 E2 51 L12 PB12 I/O FT -

##### TIM1_BKIN,

##### I2C2_SMBA,

##### SPI2_NSS/I2S2_WS,

##### SPI4_NSS/I2S4_WS,

##### SPI3_SCK/I2S3_CK,

##### EVENTOUT

##### -

##### 26 34 G1 52 K12 PB13 I/O FT -

##### TIM1_CH1N,

##### SPI2_SCK/I2S2_CK,

##### SPI4_SCK/I2S4_CK,

##### EVENTOUT

##### -

##### 27 35 F1 53 K11 PB14 I/O FT -

##### TIM1_CH2N,

##### SPI2_MISO,

```
I2S2ext_SD,
SDIO_D6,
EVENTOUT
```
##### -

##### 28 36 E1 54 K10 PB15 I/O FT -

```
RTC_50Hz,
TIM1_CH3N,
SPI2_MOSI/I2S2_SD,
SDIO_CK,
EVENTOUT
```
##### RTC_REFIN

##### - - - 55 - PD8 I/O FT - - -

##### - - - 56 K8 PD9 I/O FT - - -

##### - - - 57 J12 PD10 I/O FT - - -

##### - - - 58 J11 PD11 I/O FT - - -

##### - - - 59 J10 PD12 I/O FT -

##### TIM4_CH1,

##### EVENTOUT -

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


**Pinouts and pin description STM32F411xC STM32F411xE**

44/151 DS10314 Rev 8

##### - - - 60 H12 PD13 I/O FT -

##### TIM4_CH2,

##### EVENTOUT -

##### - - - 61 H11 PD14 I/O FT -

##### TIM4_CH3,

##### EVENTOUT -

##### - - - 62 H10 PD15 I/O FT -

##### TIM4_CH4,

##### EVENTOUT -

##### - 37 - 63 E12 PC6 I/O FT -

##### TIM3_CH1,

##### I2S2_MCK,

##### USART6_TX,

##### SDIO_D6,

##### EVENTOUT

##### -

##### - 38 - 64 E11 PC7 I/O FT -

##### TIM3_CH2,

##### SPI2_SCK/I2S2_CK,

##### I2S3_MCK,

##### USART6_RX,

##### SDIO_D7,

##### EVENTOUT

##### -

##### - 39 - 65 E10 PC8 I/O FT -

##### TIM3_CH3,

##### USART6_CK,

##### SDIO_D0,

##### EVENTOUT

##### -

##### - 40 - 66 D12 PC9 I/O FT -

##### MCO_2, TIM3_CH4,

##### I2C3_SDA,

##### I2S2_CKIN,

##### SDIO_D1,

##### EVENTOUT

##### -

##### 29 41 D1 67 D11 PA8 I/O FT -

##### MCO_1, TIM1_CH1,

##### I2C3_SCL,

##### USART1_CK,

##### USB_FS_SOF,

##### SDIO_D1,

##### EVENTOUT

##### -

##### 30 42 D2 68 D10 PA9 I/O FT -

##### TIM1_CH2,

##### I2C3_SMBA,

##### USART1_TX,

##### USB_FS_VBUS,

##### SDIO_D2,

##### EVENTOUT

##### OTG_FS_VBUS

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


```
DS10314 Rev 8 45/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
##### 31 43 C2 69 C12 PA10 I/O FT -

##### TIM1_CH3,

##### SPI5_MOSI/I2S5_SD,

##### USART1_RX,

##### USB_FS_ID,

##### EVENTOUT

##### -

##### 32 44 C1 70 B12 PA11 I/O FT -

##### TIM1_CH4,

##### SPI4_MISO,

##### USART1_CTS,

##### USART6_TX,

##### USB_FS_DM,

##### EVENTOUT

##### -

##### 33 45 C3 71 A12 PA12 I/O FT -

##### TIM1_ETR,

##### SPI5_MISO,

##### USART1_RTS,

##### USART6_RX,

##### USB_FS_DP,

##### EVENTOUT

##### -

##### 34 46 B3 72 A11 PA13 I/O FT -

##### JTMS-SWDIO,

##### EVENTOUT -

##### - - - 73 C11 VCAP_2 S - - - -

##### 35 47 B1 74 F11 VSS S - - - -

##### 36 48 B2 75 G11 VDD S - - - -

##### 37 49 A1 76 A10 PA14 I/O FT -

##### JTCK-SWCLK,

##### EVENTOUT -

##### 38 50 A2 77 A9 PA15 I/O FT -

##### JTDI,

##### TIM2_CH1/TIM2_ETR

##### ,

##### SPI1_NSS/I2S1_WS,

##### SPI3_NSS/I2S3_WS,

##### USART1_TX,

##### EVENTOUT

##### -

##### - 51 - 78 B11 PC10 I/O FT -

##### SPI3_SCK/I2S3_CK,

##### SDIO_D2,

##### EVENTOUT

##### -

##### - 52 - 79 C10 PC11 I/O FT -

```
I2S3ext_SD,
SPI3_MISO,
SDIO_D3,
EVENTOUT
```
##### -

##### - 53 - 80 B10 PC12 I/O FT -

##### SPI3_MOSI/I2S3_SD,

##### SDIO_CK,

##### EVENTOUT

##### -

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


**Pinouts and pin description STM32F411xC STM32F411xE**

46/151 DS10314 Rev 8

##### - - - 81 C9 PD0 I/O FT - EVENTOUT -

##### - - - 82 B9 PD1 I/O FT - EVENTOUT -

##### - 54 - 83 C8 PD2 I/O FT -

##### TIM3_ETR,

##### SDIO_CMD,

##### EVENTOUT

##### -

##### - - - 84 B8 PD3 I/O FT -

##### SPI2_SCK/I2S2_CK,

##### USART2_CTS,

##### EVENTOUT

##### -

##### - - - 85 B7 PD4 I/O FT -

##### USART2_RTS,

##### EVENTOUT -

##### - - - 86 A6 PD5 I/O FT -

##### USART2_TX,

##### EVENTOUT -

##### - - - 87 B6 PD6 I/O FT -

##### SPI3_MOSI/I2S3_SD,

##### USART2_RX,

##### EVENTOUT

##### -

##### - - - 88 A5 PD7 I/O FT -

##### USART2_CK,

##### EVENTOUT -

##### 39 55 A3 89 A8 PB3 I/O FT -

##### JTDO-SWO,

##### TIM2_CH2,

##### SPI1_SCK/I2S1_CK,

##### SPI3_SCK/I2S3_CK,

##### USART1_RX,

##### I2C2_SDA,

##### EVENTOUT

##### -

##### 40 56 A4 90 A7 PB4 I/O FT -

##### JTRST, TIM3_CH1,

##### SPI1_MISO,

##### SPI3_MISO,

```
I2S3ext_SD,
I2C3_SDA, SDIO_D0,
EVENTOUT
```
##### -

##### 41 57 B4 91 C5 PB5 I/O TC -

##### TIM3_CH2,

##### I2C1_SMBA,

##### SPI1_MOSI/I2S1_SD,

##### SPI3_MOSI/I2S3_SD,

##### SDIO_D3,

##### EVENTOUT

##### -

##### 42 58 C4 92 B5 PB6 I/O FT -

##### TIM4_CH1,

##### I2C1_SCL,

##### USART1_TX,

##### EVENTOUT

##### -

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


```
DS10314 Rev 8 47/151
```
**STM32F411xC STM32F411xE Pinouts and pin description**

```
57
```
##### 43 59 D4 93 B4 PB7 I/O FT -

##### TIM4_CH2,

##### I2C1_SDA,

##### USART1_RX,

##### SDIO_D0,

##### EVENTOUT

##### -

##### 44 60 A5 94 A4 BOOT0 I B - - VPP

##### 45 61 B5 95 A3 PB8 I/O FT -

##### TIM4_CH3,

##### TIM10_CH1,

##### I2C1_SCL,

##### SPI5_MOSI/I2S5_SD,

##### I2C3_SDA, SDIO_D4,

##### EVENTOUT

##### -

##### 46 62 C5 96 B3 PB9 I/O FT -

##### TIM4_CH4,

##### TIM11_CH1,

##### I2C1_SDA,

##### SPI2_NSS/I2S2_WS,

##### I2C2_SDA, SDIO_D5,

##### EVENTOUT

##### -

##### - - - 97 C3 PE0 I/O FT -

##### TIM4_ETR,

##### EVENTOUT -

##### - - - 98 A2 PE1 I/O FT - EVENTOUT -

##### 47 63 A6 99 - VSS S - - - -

##### - - B6 - H3 PDR_ON I FT - - -

##### 48 64 A7 100 - VDD S - - - -

1. Function availability depends on the chosen device.
2. PC13, PC14 and PC15 are supplied through the power switch. Since the switch only sinks a limited amount of current (3
    mA), the use of GPIOs PC13 to PC15 in output mode is limited:
    - The speed should not exceed 2 MHz with a maximum load of 30 pF.
    - These I/Os must not be used as a current source (e.g. to drive an LED).
3. Main function after the first backup domain power-up. Later on, it depends on the contents of the RTC registers even after
    reset (because these registers are not reset by the main reset). For details on how to manage these I/Os, refer to the RTC
    register description sections in the STM32F411xx reference manual.
4. FT = 5 V tolerant except when in analog mode or oscillator mode (for PC14, PC15, PH0 and PH1).
5. If the device is delivered in an UFBGA100 and the BYPASS_REG pin is set to VDD (Regulator off/internal reset ON mode),
    then PA0 is used as an internal Reset (active low)

```
Table 8. STM32F411xC/xE pin definitions (continued)
```
```
Pin number
```
```
Pin name
(function after
reset)(1) Pin type
I/O structure
```
```
Notes
```
```
Alternate functions Additional functions
```
##### UFQFPN48

##### LQFP64WLCSP49LQFP100

##### UFBGA100


**Pinouts and pin description STM32F411xC STM32F411xE**

```
48/151 DS10314 Rev 8
```
## Table 9. Alternate function mapping

```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```
```
Port A
```
```
PA0
```
-

```
TIM2_CH1/TIM2_ETR
```
```
TIM5_CH1
```
-
-
-
-

```
USART2_
```
```
CTS
```
```
-- -----
```
```
EVENTOUT
```
```
PA1
```
-

```
TIM2_CH2
```
```
TIM5_CH2
```
-
-

```
SPI4_MOSI/I2S4_SD
```
-

```
USART2_
```
```
RTS
```
```
-- -----
```
```
EVENTOUT
```
```
PA2
```
-

```
TIM2_CH3
```
```
TIM5_CH3
```
```
TIM9_CH1
```
-

```
I2S2_CKIN
```
-

```
USART2_
```
```
TX
```
```
-- -----
```
```
EVENTOUT
```
```
PA3
```
-

```
TIM2_CH4
```
```
TIM5_CH4
```
```
TIM9_CH2
```
-

```
I2S2_MCK
```
-

```
USART2_
```
```
RX
```
```
-- -----
```
```
EVENTOUT
```
```
PA4
```
-
-
-
-
-

```
SPI1_NSS/I2S1_WS
```
```
SPI3_NSS/I2
```
```
S3_WS
```
```
USART2_
```
```
CK
```
```
-- -----
```
```
EVENTOUT
```
```
PA5
```
-

```
TIM2_CH1/TIM2_ETR
```
```
---
```
```
SPI1_SCK/I
```
```
2S1_CK
```
```
---------
```
```
EVENTOUT
```
```
PA6
```
-

```
TIM1_BKIN
```
```
TIM3_CH1
```
-
-

```
SPI1_MISO
```
```
I2S2_MCK
```
-
-
-
-
-

```
SDIO_CMD
```
```
--
```
```
EVENTOUT
```
```
PA7
```
-

```
TIM1_CH1N TIM3_CH2
```
-
-

```
SPI1_MOSI/I2S1_SD
```
```
---------
```
```
EVENTOUT
```
```
PA8
```
```
MCO_1
```
```
TIM1_CH1
```
-
-

```
I2C3_SCL
```
```
--
```
```
USART1_
```
```
CK
```
```
--
```
```
USB_FS_
```
```
SOF
```
-

```
SDIO_
```
```
D1
```
```
--
```
```
EVENTOUT
```
```
PA9
```
-

```
TIM1_CH2
```
-
-

```
I2C3_SMBA
```
```
--
```
```
USART1_
```
```
TX
```
```
--
```
```
USB_FS_
```
```
VBUS
```
-

```
SDIO_
```
```
D2
```
```
--
```
```
EVENTOUT
```
```
PA10
```
-

```
TIM1_CH3
```
-
-
-
-

```
SPI5_MOSI/I
```
```
2S5_SD
```
```
USART1_
```
```
RX
```
```
--
```
```
USB_FS_
```
```
ID
```
```
----
```
```
EVENTOUT
```
```
PA11
```
-

```
TIM1_CH4
```
-
-
-
-

```
SPI4_MISO
```
```
USART1_
```
```
CTS
```
```
USART6_
```
```
TX
```
-

```
USB_FS_
```
```
DM
```
```
----
```
```
EVENTOUT
```
```
PA12
```
-

```
TIM1_ETR
```
-
-
-
-

```
SPI5_MISO
```
```
USART1_
```
```
RTS
```
```
USART6_
```
```
RX
```
-

```
USB_FS_
```
```
DP
```
```
----
```
```
EVENTOUT
```
```
PA13
```
```
JTMS-SWDIO
```
```
----- - --------
```
```
EVENTOUT
```
```
PA14
```
```
JTCK-SWCLK
```
```
----- - --------
```
```
EVENTOUT
```
```
PA15
```
```
JTDI
```
```
TIM2_CH1/TIM2_ETR
```
```
---
```
```
SPI1_NSS/I2S1_WS
```
```
SPI3_NSS/I2
```
```
S3_WS
```
```
USART1_
```
```
TX
```
```
-- -----
```
```
EVENTOUT
```

**STM32F411xC STM32F411xE Pinouts and pin description**

```
DS10314 Rev 8 49/151
```
```
Port B
```
```
PB0
```
-

```
TIM1_CH2N TIM3_CH3
```
-
-
-

```
SPI5_SCK/I2S5_CK
```
```
-- -----
```
```
EVENTOUT
```
```
PB1
```
-

```
TIM1_CH3N TIM3_CH4
```
-
-
-

```
SPI5_NSS/I2S5_WS
```
```
-- -----
```
```
EVENTOUT
```
```
PB2
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PB3
```
```
JTDO-SWO
```
```
TIM2_CH2
```
-
-
-

```
SPI1_SCK/I
```
```
2S1_CK
```
```
SPI3_SCK/I2S3_CK
```
```
USART1_
```
```
RX
```
```
-I2C2_SDA -
```
- - --

```
EVENTOUT
```
```
PB4
```
```
JTRST
```
```
TIM3_CH1
```
-
-

```
SPI1_MISO
```
```
SPI3_MISO
```
```
I2S3ext_S
```
```
D
```
```
-I2C3_SDA
```
```
SDIO_
```
```
D0
```
```
--
```
```
EVENTOUT
```
```
PB5
```
-
-

```
TIM3_CH2
```
-

```
I2C1_SMB
```
```
A
```
```
SPI1_MOSI/I2S1_SD
```
```
SPI3_MOSI/
```
```
I2S3_SD
```
```
-- --
```
```
SDIO_
```
```
D3
```
```
--
```
```
EVENTOUT
```
```
PB6
```
-
-

```
TIM4_CH1
```
-

```
I2C1_SCL
```
-
-

```
USART1_
```
```
TX
```
```
-- --
```
```
--
```
```
EVENTOUT
```
```
PB7
```
-
-

```
TIM4_CH2
```
-

```
I2C1_SDA
```
-
-

```
USART1_
```
```
RX
```
```
-- --
```
```
SDIO_
```
```
D0
```
```
--
```
```
EVENTOUT
```
```
PB8
```
-
-

```
TIM4_CH3
```
```
TIM10_CH1
```
```
I2C1_SCL
```
-

```
SPI5_MOSI/
```
```
I2S5_SD
```
-
-

```
I2C3_SDA
```
-
-

```
SDIO_
```
```
D4
```
```
--
```
```
EVENTOUT
```
```
PB9
```
-
-

```
TIM4_CH4
```
```
TIM11_CH1
```
```
I2C1_SDA
```
```
SPI2_NSS/I2S2_WS
```
```
---I2C2_SDA--
```
```
SDIO_
```
```
D5
```
```
--
```
```
EVENTOUT
```
```
PB10
```
-

```
TIM2_CH3
```
-
-

```
I2C2_SCL
```
```
SPI2_SCK/I
```
```
2S2_CK
```
```
I2S3_MCK
```
-
-
-
-
-

```
SDIO_
```
```
D7
```
```
--
```
```
EVENTOUT
```
```
PB11
```
-

```
TIM2_CH4
```
-
-

```
I2C2_SDA
```
```
I2S2_CKIN
```
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PB12
```
-

```
TIM1_BKIN
```
-
-

```
I2C2_SMB
```
```
A
```
```
SPI2_NSS/I2S2_WS
```
```
SPI4_NSS/I2S4_WS
```
```
SPI3_SCK/I2S3_CK
```
```
-- -----
```
```
EVENTOUT
```
```
PB13
```
-

```
TIM1_CH1N
```
-
-
-

```
SPI2_SCK/I
```
```
2S2_CK
```
```
SPI4_SCK/I2S4_CK
```
```
--- -----
```
```
EVENTOUT
```
```
PB14
```
-

```
TIM1_CH2N
```
-
-
-

```
SPI2_MISO
```
```
I2S2ext_SD
```
-
-
-
-
-

```
SDIO_
```
```
D6
```
```
--
```
```
EVENTOUT
```
```
PB15
```
```
RTC_50H
```
```
z
```
```
TIM1_CH3N
```
-
-
-

```
SPI2_MOSI/I2S2_SD
```
```
------
```
```
SDIO_CK
```
```
--
```
```
EVENTOUT
```
```
Table 9. Alternate function mapping (continued)
```
```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```

**Pinouts and pin description STM32F411xC STM32F411xE**

```
50/151 DS10314 Rev 8
```
```
Port C
```
```
PC0
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PC1
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PC2
```
-
-
-
-
-

```
SPI2_MISO
```
```
I2S2ext_SD
```
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PC3
```
-
-
-
-
-

```
SPI2_MOSI/I2S2_SD
```
```
---------
```
```
EVENTOUT
```
```
PC4
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PC5
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PC6
```
-
-

```
TIM3_CH1
```
-
-

```
I2S2_MCK
```
-
-

```
USART6_
```
```
TX
```
```
---
```
```
SDIO_
```
```
D6
```
```
--
```
```
EVENTOUT
```
```
PC7
```
-
-

```
TIM3_CH2
```
-
-

```
SPI2_SCK/I
```
```
2S2_CK
```
```
I2S3_MCK
```
-

```
USART6_
```
```
RX
```
```
---
```
```
SDIO_
```
```
D7
```
```
--
```
```
EVENTOUT
```
```
PC8
```
-
-

```
TIM3_CH3
```
-
-
-
-
-

```
USART6_
```
```
CK
```
```
---
```
```
SDIO_
```
```
D0
```
```
--
```
```
EVENTOUT
```
```
PC9
```
```
MCO_2
```
-

```
TIM3_CH4
```
-

```
I2C3_SDA
```
```
I2S2_CKIN
```
-
-
-
-
-

```
SDIO_
```
```
D1
```
```
--
```
```
EVENTOUT
```
```
PC10
```
-
-
-
-
-
-

```
SPI3_SCK/I2
```
```
S3_CK
```
```
--- --
```
```
SDIO_
```
```
D2
```
```
--
```
```
EVENTOUT
```
```
PC11
```
-
-
-
-
-

```
I2S3ext_SD
```
```
SPI3_MISO
```
-
-
-
-
-

```
SDIO_
```
```
D3
```
```
--
```
```
EVENTOUT
```
```
PC12
```
-
-
-
-
-
-

```
SPI3_MOSI/I
```
```
2S3_SD
```
```
--- --
```
```
SDIO_CK
```
```
--
```
```
EVENTOUT
```
```
PC13
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
PC14
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
PC15
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
Table 9. Alternate function mapping (continued)
```
```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```

**STM32F411xC STM32F411xE Pinouts and pin description**

```
DS10314 Rev 8 51/151
```
```
Port D
```
```
PD0
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD1
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD2
```
-
-

```
TIM3_ETR
```
-
-
-
-
-
-
-
-
-

```
SDIO_CMD
```
```
EVENTOUT
```
```
PD3
```
-
-
-
-
-

```
SPI2_SCK/I
```
```
2S2_CK
```
```
USART2_
```
```
CTS
```
```
-- -----
```
```
EVENTOUT
```
```
PD4
```
-
-
-
-
-
-
-

```
USART2_
```
```
RTS
```
```
-- -----
```
```
EVENTOUT
```
```
PD5
```
-
-
-
-
-
-
-

```
USART2_
```
```
TX
```
```
-- -----
```
```
EVENTOUT
```
```
PD6
```
-
-
-
-
-

```
SPI3_MOSI/I2S3_SD
```
-

```
USART2_
```
```
RX
```
```
-- -----
```
```
EVENTOUT
```
```
PD7
```
-
-
-
-
-
-
-

```
USART2_
```
```
CK
```
```
-- -----
```
```
EVENTOUT
```
```
PD8
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD9
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD10
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD11
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD12
```
-
-

```
TIM4_CH1
```
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD13
```
-
-

```
TIM4_CH2
```
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD14
```
-
-

```
TIM4_CH3
```
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PD15
```
-
-

```
TIM4_CH4
```
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
Table 9. Alternate function mapping (continued)
```
```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```

**Pinouts and pin description STM32F411xC STM32F411xE**

```
52/151 DS10314 Rev 8
```
```
Port E
```
```
PE0
```
-
-

```
TIM4_ETR
```
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE1
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE2
```
```
TRACECL
```
```
K
```
```
----
```
```
SPI4_SCK/I
```
```
2S4_CK
```
```
SPI5_SCK/I2
```
```
S5_CK
```
```
--- -----
```
```
EVENTOUT
```
```
PE3
```
```
TRACED0
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE4
```
```
TRACED1
```
-
-
-
-

```
SPI4_NSS/I2S4_WS
```
```
SPI5_NSS/I2
```
```
S5_WS
```
```
--- -----
```
```
EVENTOUT
```
```
PE5
```
```
TRACED2
```
-
-

```
TIM9_CH1
```
-

```
SPI4_MISO
```
```
SPI5_MISO
```
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE6
```
```
TRACED3
```
-
-

```
TIM9_CH2
```
-

```
SPI4_MOSI/I2S4_SD
```
```
SPI5_MOSI/I
```
```
2S5_SD
```
```
--- -----
```
```
EVENTOUT
```
```
PE7
```
-

```
TIM1_ETR
```
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE8
```
-

```
TIM1_CH1N
```
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE9
```
-

```
TIM1_CH1
```
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE10
```
-

```
TIM1_CH2N
```
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE11
```
-

```
TIM1_CH2
```
-
-
-

```
SPI4_NSS/I2S4_WS
```
```
SPI5_NSS/I2
```
```
S5_WS
```
```
--- -----
```
```
EVENTOUT
```
```
PE12
```
-

```
TIM1_CH3N
```
-
-
-

```
SPI4_SCK/I
```
```
2S4_CK
```
```
SPI5_SCK/I2
```
```
S5_CK
```
```
--- -----
```
```
EVENTOUT
```
```
PE13
```
-

```
TIM1_CH3
```
-
-
-

```
SPI4_MISO
```
```
SPI5_MISO
```
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
PE14
```
-

```
TIM1_CH4
```
-
-
-

```
SPI4_MOSI/I2S4_SD
```
```
SPI5_MOSI/I
```
```
2S5_SD
```
```
--- -----
```
```
EVENTOUT
```
```
PE15
```
-

```
TIM1_BKIN
```
-
-
-
-
-
-
-
-
-
-
-
-
-

```
EVENTOUT
```
```
Table 9. Alternate function mapping (continued)
```
```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```

**STM32F411xC STM32F411xE Pinouts and pin description**

```
DS10314 Rev 8 53/151
```
```
Port H
```
```
PH0
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
PH1
```
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-
-

```
Table 9. Alternate function mapping (continued)
```
```
Port
```
```
AF00
```
```
AF01
```
```
AF02
```
```
AF03
```
```
AF04
```
```
AF05
```
```
AF06
```
```
AF07
```
```
AF08
```
```
AF09
```
```
AF10
```
```
AF11
```
```
AF12
```
```
AF13 AF14
```
```
AF15
```
```
SYS_AF
```
```
TIM1/TIM2
```
```
TIM3/
TIM4/ TIM5
```
```
TIM9/ TIM10/ TIM11
```
```
I2C1/I2C2/
```
```
I2C3
```
```
SPI1/I2S1S
```
```
PI2/
I2S2/SPI3/
```
```
I2S3
```
```
SPI2/I2S2/
```
```
SPI3/
I2S3/SPI4/ I2S4/SPI5/
```
```
I2S5
```
```
SPI3/I2S3/ USART1/ USART2
```
```
USART6
```
```
I2C2/I2C3
```
```
OTG1_FS
```
```
SDIO
```

**Memory mapping STM32F411xC STM32F411xE**

54/151 DS10314 Rev 8

## 5 Memory mapping

```
The memory map is shown in Figure 14.
```
## Figure 14. Memory map

```
Table 10. STM32F411xC/xE
register boundary addresses
```
```
Bus Boundary address Peripheral
```
```
0xE010 0000 - 0xFFFF FFFF Reserved
```
```
Cortex®-M4 0xE000 0000 - 0xE00F FFFF Cortex-M4 internal peripherals
```
```
0x5004 0000 - 0xDFFF FFFF Reserved
```
```
MSv34706V1
```
```
512-Mbyte
block 7
Cortex-M4's
internal
peripherals
512-Mbyte
block 6
Not used
```
```
512-Mbyte
block 2
Peripherals
```
```
512-Mbyte
block 1
SRAM
```
```
0x0000 0000
```
```
0x1FFF FFFF
```
```
0x2000 0000
```
```
0x3FFF FFFF
```
```
0x4000 0000
```
```
0x5FFF FFFF
```
```
0x6000 0000
```
```
0xC000 0000
```
```
0xDFFF FFFF
```
```
0xE000 0000
```
```
0xFFFF FFFF
```
```
512-Mbyte
block 0
Code
```
```
0x2002 0001 - 0x3FFF FFFF
```
```
0x4000 0000
```
```
Reserved
0x4000 73FF
```
```
0x4000 7400 - 0x4000 FFFF
```
```
0x4001 0000
```
```
0x5004 0000
```
```
0xDFFF FFFF
```
```
AHB2
```
```
Reserved
0x5003 FFFF
```
```
0x5000 0000
```
```
SRAM (128 KB aliasedby bit-banding) 0x2000 0000 - 0x2002 0000
```
```
APB1
```
```
APB2
```
```
0x4001 4BFF
```
```
Reserved 0x4001 4C00 - 0x4001 FFFF
```
```
0x4002 6800 - 0x4FFF FFFF
0x4002 67FF
```
```
AHB1
```
```
Reserved
```
```
Flash memory
```
```
0x0808 0000 - 0x1FFE FFFF
```
```
0x1FFF C000 - 0x1FFF C007
```
```
0x0800 0000 - 0x0807 FFFF
0x0008 0000 - 0x07FF FFFF
```
```
0x0000 0000 - 0x0007 FFFF
```
```
Reserved
Reserved
Aliased to Flash,
system, memory or
SRAM depending, on
the BOOT pins
```
```
System memory
```
```
0x1FFF C008 - 0x1FFF FFFF
```
```
0x1FFF 7A10 - 0x1FFF BFFF
0x1FFF 0000 - 0x1FFF 7A0F
```
```
Option bytes
```
```
0x4002 0000
```
```
Cortex-M4 internal peripherals 0xE000 0000 - 0xE00F FFFF
```
```
Reserved 0xE010 0000 - 0xFFFF FFFF
```
```
Reserved
```
```
0xBFFF FFFF
```
```
Reserved
```
```
Reserved
```
```
Reserved
```

```
DS10314 Rev 8 55/151
```
**STM32F411xC STM32F411xE Memory mapping**

```
57
```
```
AHB2 0x5000 0000 - 0x5003 FFFF USB OTG FS
```
##### AHB1

```
0x4002 6800 - 0x4FFF FFFF Reserved
```
```
0x4002 6400 - 0x4002 67FF DMA2
```
```
0x4002 6000 - 0x4002 63FF DMA1
```
```
0x4002 5000 - 0x4002 4FFF Reserved
```
```
0x4002 3C00 - 0x4002 3FFF Flash interface register
```
```
0x4002 3800 - 0x4002 3BFF RCC
```
```
0x4002 3400 - 0x4002 37FF Reserved
```
```
0x4002 3000 - 0x4002 33FF CRC
```
```
0x4002 2000 - 0x4002 2FFF Reserved
```
```
0x4002 1C00 - 0x4002 1FFF GPIOH
```
```
0x4002 1400 - 0x4002 1BFF Reserved
```
```
0x4002 1000 - 0x4002 13FF GPIOE
```
```
0x4002 0C00 - 0x4002 0FFF GPIOD
```
```
0x4002 0800 - 0x4002 0BFF GPIOC
```
```
0x4002 0400 - 0x4002 07FF GPIOB
```
```
0x4002 0000 - 0x4002 03FF GPIOA
```
```
Table 10. STM32F411xC/xE
register boundary addresses (continued)
```
```
Bus Boundary address Peripheral
```

**Memory mapping STM32F411xC STM32F411xE**

56/151 DS10314 Rev 8

##### APB2

```
0x4001 5400- 0x4001 FFFF Reserved
```
```
0x4001 5000 - 0x4001 53FFF SPI5/I2S5
```
```
0x4001 4800 - 0x4001 4BFF TIM11
```
```
0x4001 4400 - 0x4001 47FF TIM10
```
```
0x4001 4000 - 0x4001 43FF TIM9
```
```
0x4001 3C00 - 0x4001 3FFF EXTI
```
```
0x4001 3800 - 0x4001 3BFF SYSCFG
```
```
0x4001 3400 - 0x4001 37FF SPI4/I2S4
```
```
0x4001 3000 - 0x4001 33FF SPI1/I2S1
```
```
0x4001 2C00 - 0x4001 2FFF SDIO
```
```
0x4001 2400 - 0x4001 2BFF Reserved
```
```
0x4001 2000 - 0x4001 23FF ADC1
```
```
0x4001 1800 - 0x4001 1FFF Reserved
```
```
0x4001 1400 - 0x4001 17FF USART6
```
```
0x4001 1000 - 0x4001 13FF USART1
```
```
0x4001 0400 - 0x4001 0FFF Reserved
```
```
0x4001 0000 - 0x4001 03FF TIM1
```
```
0x4000 7400 - 0x4000 FFFF Reserved
```
```
Table 10. STM32F411xC/xE
register boundary addresses (continued)
```
```
Bus Boundary address Peripheral
```

```
DS10314 Rev 8 57/151
```
**STM32F411xC STM32F411xE Memory mapping**

```
57
```
##### APB1

```
0x4000 7000 - 0x4000 73FF PWR
```
```
0x4000 6000 - 0x4000 6FFF Reserved
```
```
0x4000 5C00 - 0x4000 5FFF I2C3
```
```
0x4000 5800 - 0x4000 5BFF I2C2
```
```
0x4000 5400 - 0x4000 57FF I2C1
```
```
0x4000 4800 - 0x4000 53FF Reserved
```
```
0x4000 4400 - 0x4000 47FF USART2
```
```
0x4000 4000 - 0x4000 43FF I2S3ext
```
```
0x4000 3C00 - 0x4000 3FFF SPI3 / I2S3
```
```
0x4000 3800 - 0x4000 3BFF SPI2 / I2S2
```
```
0x4000 3400 - 0x4000 37FF I2S2ext
```
```
0x4000 3000 - 0x4000 33FF IWDG
```
```
0x4000 2C00 - 0x4000 2FFF WWDG
```
```
0x4000 2800 - 0x4000 2BFF RTC & BKP Registers
```
```
0x4000 1000 - 0x4000 27FF Reserved
```
```
0x4000 0C00 - 0x4000 0FFF TIM5
```
```
0x4000 0800 - 0x4000 0BFF TIM4
```
```
0x4000 0400 - 0x4000 07FF TIM3
```
```
0x4000 0000 - 0x4000 03FF TIM2
```
```
Table 10. STM32F411xC/xE
register boundary addresses (continued)
```
```
Bus Boundary address Peripheral
```

**Electrical characteristics STM32F411xC STM32F411xE**

58/151 DS10314 Rev 8

## 6 Electrical characteristics

### 6.1 Parameter conditions

```
Unless otherwise specified, all voltages are referenced to VSS.
```
#### 6.1.1 Minimum and maximum values

```
Unless otherwise specified the minimum and maximum values are guaranteed in the worst
conditions of ambient temperature, supply voltage and frequencies by tests in production on
100% of the devices with an ambient temperature at TA = 25 °C and TA = TAmax (given by
the selected temperature range).
```
```
Data based on characterization results, design simulation and/or technology characteristics
are indicated in the table footnotes and are not tested in production. Based on
characterization, the minimum and maximum values refer to sample tests and represent the
mean value plus or minus three times the standard deviation (mean ±3 σ).
```
#### 6.1.2 Typical values

```
Unless otherwise specified, typical data are based on TA = 25 °C, VDD = 3.3 V (for the
1.7 V ≤ VDD ≤ 3.6 V voltage range). They are given only as design guidelines and are not
tested.
```
```
Typical ADC accuracy values are determined by characterization of a batch of samples from
a standard diffusion lot over the full temperature range, where 95% of the devices have an
error less than or equal to the value indicated (mean ±2 σ).
```
#### 6.1.3 Typical curves

```
Unless otherwise specified, all typical curves are given only as design guidelines and are
not tested.
```
#### 6.1.4 Loading capacitor

```
The loading conditions used for pin parameter measurement are shown in Figure 15.
```
## Figure 15. Pin loading conditions

```
MS19011V2
```
```
C = 50 pF
```
```
MCU pin
```

```
DS10314 Rev 8 59/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.1.5 Pin input voltage

```
The input voltage measurement on a pin of the device is described in Figure 16.
```
## Figure 16. Input voltage measurement

```
MS19010V2
```
```
MCU pin
```
```
VIN
```

**Electrical characteristics STM32F411xC STM32F411xE**

60/151 DS10314 Rev 8

#### 6.1.6 Power supply scheme

## Figure 17. Power supply scheme

1. To connect PDR_ON pin, refer to _Section 3.15: Power supply supervisor_.
2. The 4.7 μF ceramic capacitor must be connected to one of the VDD pin.
3. VCAP_2 pad is only available on LQFP100 and UFBGA100 packages.
4. VDDA=VDD and VSSA=VSS.

**Caution:** Each power supply pair (for example VDD/VSS, VDDA/VSSA) must be decoupled with filtering
ceramic capacitors as shown above. These capacitors must be placed as close as possible
to, or below, the appropriate pins on the underside of the PCB to ensure good operation of
the device. It is not recommended to remove filtering capacitors to reduce PCB size or cost.
This might cause incorrect operation of the device.

```
069
```
```
%DFNXSFLUFXLWU\
26&.57&
:DNHXSORJLF
%DFNXSUHJLVWHUV
```
```
.HUQHOORJLF
&38GLJLWDO
	5$0
```
```
$QDORJ
5&V
3//
```
```
3RZHU
VZLWFK
```
```
9%$7
```
```
*3,2V
```
```
287
```
```
,1
```
```
îQ)
î)
```
```
9%$7
WR9
```
```
9ROWDJH
UHJXODWRU
```
```
9''$
```
```
$'&
```
```
/HYHOVKLIWHU
```
```
,2
/RJLF
```
```
9''
```
```
Q)
)
```
```
)ODVKPHPRU\
```
```
9&$3B
```
```
%<3$66B5(*
```
```
3'5B21
```
```
5HVHW
FRQWUROOHU
```
```
9''

966
 
```
```
9''
```
```
95()
```
```
95()
```
```
966$
```
```
95()
```
```
Q)
)
```
```
9&$3B
î) î)RU
```

```
DS10314 Rev 8 61/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.1.7 Current consumption measurement

## Figure 18. Current consumption measurement scheme

### 6.2 Absolute maximum ratings

```
Stresses above the absolute maximum ratings listed in Table 11: Voltage characteristics ,
Table 12: Current characteristics , and Table 13: Thermal characteristics may cause
permanent damage to the device. These are stress ratings only and functional operation of
the device at these conditions is not implied. Exposure to maximum rating conditions for
extended periods may affect device reliability.
```
```
ai14126
```
```
VBAT
```
```
VDD
```
```
VDDA
```
```
IDD_VBAT
```
```
IDD
```
## Table 11. Voltage characteristics

```
Symbol Ratings Min Max Unit
```
```
VDD–VSS External main supply voltage (including VDDA,^ VDD and^
VBAT)(1)
```
1. All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external power
    supply, in the permitted range.

##### –0.3 4.0

##### V

##### VIN

```
Input voltage on FT and TC pins(2)
```
2. VIN maximum value must always be respected. Refer to _Table 12_ for the values of the maximum allowed
    injected current.

##### VSS–0.3 VDD+4.0

```
Input voltage on any other pin VSS–0.3 4.0
```
```
Input voltage for BOOT0 VSS 9.0
```
```
|ΔVDDx| Variations between different VDD power pins - 50
mV
|VSSX −VSS| Variations between all the different ground pins - 50
```
```
VESD(HBM) Electrostatic discharge voltage (human body model)
```
```
see Section 6.3.14:
Absolute maximum
ratings (electrical
sensitivity)
```

**Electrical characteristics STM32F411xC STM32F411xE**

62/151 DS10314 Rev 8

## Table 12. Current characteristics

```
Symbol Ratings Max. Unit
```
```
ΣIVDD Total current into sum of all VDD_x power lines (source)(1) 160
```
```
mA
```
```
Σ IVSS Total current out of sum of all VSS_x ground lines (sink)(1) -160
```
```
IVDD Maximum current into each VDD_x power line (source)(1) 100
```
```
IVSS Maximum current out of each VSS_x ground line (sink)(1) -100
```
##### IIO

```
Output current sunk by any I/O and control pin 25
```
```
Output current sourced by any I/O and control pin -25
```
##### ΣIIO

```
Total output current sunk by sum of all I/O and control pins (2) 120
```
```
Total output current sourced by sum of all I/Os and control pins(2) -120
```
##### IINJ(PIN) (3)

```
Injected current on FT and TC pins (4)
–5/+0
Injected current on NRST and B pins (4)
```
```
ΣIINJ(PIN) Total injected current (sum of all I/O and control pins)(5) ±25
```
1. All main power (VDD, VDDA) and ground (VSS, VSSA) pins must always be connected to the external power supply, in the
    permitted range.
2. This current consumption must be correctly distributed over all I/Os and control pins.
3. Negative injection disturbs the analog performance of the device. See note in _Section 6.3.20: 12-bit ADC characteristics_.
4. Positive injection is not possible on these I/Os and does not occur for input voltages lower than the specified maximum
    value.
5. When several inputs are submitted to a current injection, the maximum ΣIINJ(PIN) is the absolute sum of the positive and
    negative injected currents (instantaneous values).

## Table 13. Thermal characteristics.

```
Symbol Ratings Value Unit
```
```
TSTG Storage temperature range –65 to +150
```
##### °C

```
TJ Maximum junction temperature 130
```
##### TLEAD

```
Maximum lead temperature during soldering
(WLCSP49, LQFP64/100, UFQFPN48,
UFBGA100)
```
```
see note (1)
```
1. Compliant with JEDEC Std J-STD-020D (for small body, Sn-Pb or Pb assembly), the ST ECOPACK®
    7191395 specification, and the European directive on Restrictions on Hazardous Substances (ROHS
    directive 2011/65/EU, July 2011).


```
DS10314 Rev 8 63/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
### 6.3 Operating conditions

#### 6.3.1 General operating conditions

## Table 14. General operating conditions

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fHCLK Internal AHB clock frequency
```
```
Power Scale3: Regulator ON,
VOS[1:0] bits in PWR_CR register = 0x01
```
##### 0-64

```
MHz
```
```
Power Scale2: Regulator ON,
VOS[1:0] bits in PWR_CR register = 0x10
```
##### 0 - 84

```
Power Scale1: Regulator ON,
VOS[1:0] bits in PWR_CR register = 0x11
```
##### 0-100

```
fPCLK1 Internal APB1 clock frequency 0 - 50 MHz
```
```
fPCLK2 Internal APB2 clock frequency 0 - 100 MHz
```
```
VDD Standard operating voltage 1.7(1) -3.6V
```
##### VDDA(2)(3)

```
Analog operating voltage
(ADC limited to 1.2 M
samples)
Must be the same potential as VDD(4)
```
##### 1.7(1) -2.4

##### V

```
Analog operating voltage
(ADC limited to 2.4 M
samples)
```
##### 2.4 - 3.6

```
VBAT Backup operating voltage 1.65 - 3.6 V
```
##### V 12

```
Regulator ON: 1.2 V internal
voltage on VCAP_1/VCAP_2
pins
```
```
VOS[1:0] bits in PWR_CR register = 0x01
Max frequency 64 MHz
```
##### 1.08

##### (5) 1.14 1.20

```
(5)
```
##### V

```
VOS[1:0] bits in PWR_CR register = 0x10
Max frequency 84 MHz
```
##### 1.20

##### (5) 1.26 1.32

```
(5)
```
```
VOS[1:0] bits in PWR_CR register = 0x11
Max frequency 100 MHz
```
##### 1.26 1.32 1.38

##### V 12

```
Regulator OFF: 1.2 V external
voltage must be supplied on
VCAP_1/VCAP_2 pins
```
```
Max frequency 64 MHz 1.10 1.14 1.20
```
```
Max frequency 84 MHz 1.20 1.26 1.32 V
```
```
Max frequency 100 MHz 1.26 1.32 1.38
```
##### VIN

```
Input voltage on RST, FT and
TC pins(6)
```
##### 2 V ≤ VDD ≤ 3.6 V –0.3 - 5.5

##### VDD ≤ 2 V –0.3 - 5.2 V

```
Input voltage on BOOT0 pin - 0 - 9
```
##### PD

```
Power dissipation at
TA = 85°C (range 6) or
105 °C (range 7)(7)
```
##### UFQFPN48 - - 625

```
mW
```
##### WLCSP49 - - 392

##### LQFP64 - - 425

##### LQFP100 - - 465

##### UFBGA100 - - 323


**Electrical characteristics STM32F411xC STM32F411xE**

64/151 DS10314 Rev 8

```
PD Power dissipation at
TA = 125 °C (range 3)(7)
```
##### UFQFPN48 - - 156

```
mW
```
##### WLCSP49 - - 98

##### LQFP64 - - 106

##### LQFP100 - - 116

##### UFBGA100 - - 81

##### TA

```
Ambient temperature for
range 6
```
```
Maximum power dissipation - 40 - 85
```
##### °C

```
Low power dissipation(8) - 40 - 105
```
```
Ambient temperature for
range 7
```
```
Maximum power dissipation - 40 - 105
```
```
Low power dissipation(8) - 40 - 125
```
```
Ambient temperature for
range 3
```
```
Maximum power dissipation - 40 - 110
```
```
Low power dissipation(8) - 40 - 130
```
```
TJ Junction temperature range
```
```
Range 6 - 40 - 105
```
```
Range 7 - 40 - 125
```
```
Range 3 - 40 - 130
```
1. VDD/VDDA minimum value of 1.7 V with the use of an external power supply supervisor (refer to _Section 3.15.2: Internal_
    _reset OFF_ ).
2. When the ADC is used, refer to _Table 65: ADC characteristics_.
3. If VREF+ pin is present, it must respect the following condition: VDDA-VREF+ < 1.2 V.
4. It is recommended to power VDD and VDDA from the same source. A maximum difference of 300 mV between VDD and
    VDDA can be tolerated during power-up and power-down operation.
5. Guaranteed by test in production.
6. To sustain a voltage higher than VDD+0.3, the internal Pull-up and Pull-Down resistors must be disabled
7. If TA is lower, higher PD values are allowed as long as TJ does not exceed TJmax.
8. In low power dissipation state, TA can be extended to this range as long as TJ does not exceed TJmax.

## Table 16. VCAP_1/VCAP_2 operating conditions

```
Symbol Parameter Conditions Min Typ Max Unit
```
## Table 15. Features depending on the operating power supply range

```
Operating
power
supply
range
```
##### ADC

```
operation
```
```
Maximum
flash memory
access
frequency
with no wait
states
(fFlashmax)
```
```
Maximum flash
memory access
frequency with
wait states (1)(2)
```
```
I/O operation
```
```
Clock output
frequency on
I/O pins(3)
```
```
Possible
flash
memory
operations
```
```
VDD =1.7 to
2.1 V(4)
```
```
Conversion
time up to
1.2 Msps
```
```
16 MHz(5) 100 MHz with 6
wait states
```
- No I/O
    compensation

```
up to 30 MHz
```
```
8-bit erase
and program
operations
only
```
```
VDD = 2.1 to
2.4 V
```
```
Conversion
time up to
1.2 Msps
```
```
18 MHz
```
```
100 MHz with 5
wait states
```
- No I/O
    compensation up to 30 MHz

```
16-bit erase
and program
operations
```

```
DS10314 Rev 8 65/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.2 VCAP_1/VCAP_2 external capacitors

```
Stabilization for the main regulator is achieved by connecting the external capacitor CEXT to
the VCAP_1 and VCAP_2 pins. For packages supporting only 1 VCAP pin, the 2 CEXT
capacitors are replaced by a single capacitor.
```
```
CEXT is specified in Table 16.
```
## Figure 19. External capacitor CEXT

1. Legend: ESR is the equivalent series resistance.

```
VDD = 2.4 to
2.7 V
```
```
Conversion
time up to
2.4 Msps
```
```
24 MHz 100 MHz with 4
wait states
```
##### – I/O

```
compensation
works
```
```
up to 50 MHz
```
```
16-bit erase
and program
operations
```
```
VDD = 2.7 to
3.6 V(6)
```
```
Conversion
time up to
2.4 Msps
```
```
30 MHz
```
```
100 MHz with 3
wait states
```
##### – I/O

```
compensation
works
```
```
–up to
100 MHz
when VDD =
3.0 to 3.6 V
–up to
50 MHz
when VDD =
2.7 to 3.0 V
```
```
32-bit erase
and program
operations
```
1. Applicable only when the code is executed from flash memory. When the code is executed from RAM, no wait state is
    required.
2. Thanks to the ART accelerator and the 128-bit flash memory, the number of wait states given here does not impact the
    execution speed from flash memory since the ART accelerator allows to achieve a performance equivalent to 0 wait state
    program execution.
3. Refer to _Table 55: I/O AC characteristics_ for frequencies vs. external load.
4. VDD/VDDA minimum value of 1.7 V, with the use of an external power supply supervisor (refer to _Section 3.15.2: Internal_
    _reset OFF_ ).
5. Prefetch is not available. Refer to AN3430 application note for details on how to adjust performance and power.
6. The voltage range for the USB full speed embedded PHY can drop down to 2.7 V. However the electrical characteristics of
    D- and D+ pins will be degraded between 2.7 and 3 V.

```
Table 15. Features depending on the operating power supply range (continued)
```
```
Operating
power
supply
range
```
##### ADC

```
operation
```
```
Maximum
flash memory
access
frequency
with no wait
states
(fFlashmax)
```
```
Maximum flash
memory access
frequency with
wait states (1)(2)
```
```
I/O operation
```
```
Clock output
frequency on
I/O pins(3)
```
```
Possible
flash
memory
operations
```
```
069
```
```
(65
```
```
5/HDN
```
```
&
```

**Electrical characteristics STM32F411xC STM32F411xE**

66/151 DS10314 Rev 8

#### 6.3.3 Operating conditions at power-up/power-down (regulator ON)

```
Subject to general operating conditions for TA.
```
## Table 17. Operating conditions at power-up / power-down (regulator ON)

#### 6.3.4 Operating conditions at power-up / power-down (regulator OFF)

```
Subject to general operating conditions for TA.
```
_Note: This feature is only available for UFBGA100 package._

```
Table 16. VCAP_1/VCAP_2 operating conditions(1)
```
1. When bypassing the voltage regulator, the two 2.2 μF VCAP capacitors are not required and should be
    replaced by two 100 nF decoupling capacitors.

```
Symbol Parameter Conditions
```
##### CEXT

```
Capacitance of external capacitor with a single VCAP
pin available 4.7 μF
```
##### ESR

```
ESR of external capacitor with a single VCAP pin
available < 1 Ω
```
```
Symbol Parameter Min Max Unit
```
```
tVDD
```
```
VDD rise time rate 20 ∞
μs/V
VDD fall time rate 20 ∞
```
## Table 18. Operating conditions at power-up / power-down (regulator OFF).

1. To reset the internal logic at power-down, a reset must be applied on pin PA0 when VDD reach below
    1.08 V.

```
Symbol Parameter Conditions Min Max Unit
```
```
tVDD
```
```
VDD rise time rate Power-up 20 ∞
```
```
μs/V
```
```
VDD fall time rate Power-down 20 ∞
```
```
tVCAP
```
```
VCAP_1 and VCAP_2 rise time rate Power-up 20 ∞
```
```
VCAP_1 and VCAP_2 fall time rate Power-down 20 ∞
```

```
DS10314 Rev 8 67/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.5 Embedded reset and power control block characteristics

```
The parameters given in Table 19 are derived from tests performed under ambient
temperature and VDD supply voltage @ 3.3V.
```
## Table 19. Embedded reset and power control block characteristics.

```
Symbol Parameter Conditions Min Typ^ Max Unit
```
##### VPVD

```
Programmable voltage
detector level selection
```
```
PLS[2:0]=000 (rising edge) 2.09 2.14 2.19
```
##### V

```
PLS[2:0]=000 (falling edge) 1.98 2.04 2.08
```
```
PLS[2:0]=001 (rising edge) 2.23 2.30 2.37
```
```
PLS[2:0]=001 (falling edge) 2.13 2.19 2.25
```
```
PLS[2:0]=010 (rising edge) 2.39 2.45 2.51
```
```
PLS[2:0]=010 (falling edge) 2.29 2.35 2.39
```
```
PLS[2:0]=011 (rising edge) 2.54 2.60 2.65
```
```
PLS[2:0]=011 (falling edge) 2.44 2.51 2.56
```
```
PLS[2:0]=100 (rising edge) 2.70 2.76 2.82
```
```
PLS[2:0]=100 (falling edge) 2.59 2.66 2.71
```
```
PLS[2:0]=101 (rising edge) 2.86 2.93 2.99
```
```
PLS[2:0]=101 (falling edge) 2.77 2.82 2.89
```
```
PLS[2:0]=110 (rising edge) 2.96 3.03 3.10
```
```
PLS[2:0]=110 (falling edge) 2.85 2.93 2.99
```
```
PLS[2:0]=111 (rising edge) 3.07 3.14 3.21
```
```
PLS[2:0]=111 (falling edge) 2.95 3.03 3.09
```
```
VPVDhyst(2) PVD hysteresis - - 100 - mV
```
```
VPOR/PDR Power-on/power-down
reset threshold
```
```
Falling edge 1.60(1) 1.68 1.76
V
Rising edge 1.64 1.72 1.80
```
```
VPDRhyst(2) PDR hysteresis - - 40 - mV
```
```
VBOR1 Brownout level 1
threshold
```
```
Falling edge 2.13 2.19 2.24
```
##### V

```
Rising edge 2.23 2.29 2.33
```
```
VBOR2 Brownout level 2
threshold
```
```
Falling edge 2.44 2.50 2.56
```
```
Rising edge 2.53 2.59 2.63
```
```
VBOR3 Brownout level 3
threshold
```
```
Falling edge 2.75 2.83 2.88
```
```
Rising edge 2.85 2.92 2.97
```
```
VBORhyst(2) BOR hysteresis - - 100 - mV
```
```
TRSTTEMPO
(2)(3) POR reset timing - 0.5 1.5 3.0 ms
```

**Electrical characteristics STM32F411xC STM32F411xE**

68/151 DS10314 Rev 8

#### 6.3.6 Supply current characteristics

```
The current consumption is a function of several parameters and factors such as the
operating voltage, ambient temperature, I/O pin loading, device software configuration,
operating frequencies, I/O pin switching rate, program location in memory and executed
binary code.
The current consumption is measured as described in Figure 18: Current consumption
measurement scheme.
```
```
All the run-mode current consumption measurements given in this section are performed
with a reduced code that gives a consumption equivalent to CoreMark code.
```
**Typical and maximum current consumption**

```
The MCU is placed under the following conditions:
```
- All I/O pins are in input mode with a static value at VDD or VSS (no load).
- All peripherals are disabled except if it is explicitly mentioned.
- The flash memory access time is adjusted to both fHCLK frequency and VDD ranges
    (refer to _Table 15: Features depending on the operating power supply range_ ).
- The voltage scaling is adjusted to fHCLK frequency as follows:
    - Scale 3 for fHCLK ≤ 64 MHz
    - Scale 2 for 64 MHz < fHCLK ≤ 84 MHz
    - Scale 1 for 84 MHz < fHCLK ≤ 100 MHz
- The system clock is HCLK, fPCLK1 = fHCLK/2, and fPCLK2 = fHCLK.
- External clock is 4 MHz and PLL is ON except if it is explicitly mentioned.
- The maximum values are obtained for VDD = 3.6 V and a maximum ambient
    temperature (TA), and the typical values for TA= 25 °C and VDD = 3.3 V unless
    otherwise specified.

##### IRUSH(2)

```
In-Rush current on
voltage regulator power-
on (POR or wakeup from
Standby)
```
- - 160 200 mA

##### ERUSH(2)

```
In-Rush energy on
voltage regulator power-
on (POR or wakeup from
Standby)
```
##### VDD = 1.7 V, TA = 125 °C,

```
IRUSH = 171 mA for 31 μs --5.4μC
```
1. The product behavior is guaranteed by design down to the minimum VPOR/PDR value.
2. Guaranteed by design - Not tested in production.
3. The reset timing is measured from the power-on (POR reset or wakeup from VBAT) to the instant when first
    instruction is fetched by the user application code.

```
Table 19. Embedded reset and power control block characteristics (continued)
```
```
Symbol Parameter Conditions Min Typ^ Max Unit
```

```
DS10314 Rev 8 69/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
Table 20. Typical and maximum current consumption, code with data processing (ART
accelerator disabled) running from SRAM - VDD = 1.7 V
```
```
Symbol Parameter Conditions fHCLK^
(MHz)
```
```
Typ Max(1)
```
```
T Unit
A=
25 °C
```
##### TA=

##### 25 °C

##### TA=

##### 85 °C

##### TA=

##### 105 °C

##### TA=

##### 125 °C

##### IDD

```
Supply
current in Run
mode
```
```
External clock,
PLL ON(2), all
peripherals
enabled(3)(4)
```
##### 100 21.4 23.0 23.6 24.0 25.0

```
mA
```
##### 84 17.2 18.9(5) 19.1 19.2 20.2

##### 64 11.9 12.9 13.2 13.7 14.6

##### 50 9.4 10.1 10.4 11.0 11.9

##### 20 4.3 4.8 5.0 5.6 6.5

```
HSI, PLL off, all
peripherals
enabled(4)
```
##### 16 3.0 3.3 3.6 4.3 5.2

##### 1 0.5 0.7 1.0 1.7 2.6

```
External clock,
PLL on (2))all
peripherals
disabled (3)
```
##### 100 12.7 14.0 14.4 14.8 15.8

##### 84 10.2 11.6(5) 11.8 12.0 13.0

##### 64 7.1 7.9 8.2 8.7 9.7

##### 50 5.6 6.3 6.5 7.1 8.0

##### 20 2.5 3.0 3.3 3.9 4.8

```
HSI, PLL off, all
peripherals
disabled(4)
```
##### 16 1.9 2.1 2.4 3.0 3.9

##### 1 0.4 0.5 0.9 1.6 2.5

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. When analog peripheral blocks such as ADC, HSE, LSE, HSI, or LSI are ON, an additional power consumption has to be
    considered.
4. When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA for the
    analog part.
5. Guaranteed by test in production.


**Electrical characteristics STM32F411xC STM32F411xE**

70/151 DS10314 Rev 8

```
Table 21. Typical and maximum current consumption, code with data processing (ART
accelerator disabled) running from SRAM - VDD = 3.6 V
```
```
Symbol Parameter Conditions fHCLK^
(MHz)
```
```
Typ
```
```
Max(1)
```
```
T Unit
A=
25 °C
```
##### TA=

##### 85 °C

##### TA=

##### 105 °C

##### TA=

##### 125 °C

##### IDD

```
Supply
current in
Run mode
```
```
External clock,
PLL ON(2), all
peripherals
enabled(3)(4)
```
##### 100 21.7 23.3 23.9 24.3 25.3

```
mA
```
##### 84 17.5 19.2(5) 19.4 19.5 20.5

##### 64 12.2 13.2 13.5 14.0 14.9

##### 50 9.6 10.4 10.7 11.2 12.1

##### 20 4.5 5.0 5.3 5.9 6.8

```
HSI, PLL OFF, all
peripherals
enabled (3)
```
##### 16 3.0 3.3 3.6 4.3 5.2

##### 1 0.5 0.7 1.0 1.7 2.6

```
External clock,
PLL OFF(2),
all peripherals
disabled (3)
```
##### 100 13.0 14.6(5) 14.6 14.9 16.0

##### 84 10.5 11.9(5) 12.1 12.2 13.2

##### 64 7.4 8.4(5) 8.8 8.9 9.9

##### 50 5.9 6.6 6.8 7.3 8.2

##### 20 2.8 3.3 3.5 4.2 5.1

```
HSI, PLL OFF, all
peripherals
disabled (3)
```
##### 16 1.9 2.1 2.4 3.1 4.0

##### 1 0.4 0.5 0.9 1.6 2.5

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. When analog peripheral blocks such as ADC, HSE, LSE, HSI, or LSI are ON, an additional power consumption has to be
    considered.
4. When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6 mA for the
    analog part.
5. Guaranteed by test in production.


```
DS10314 Rev 8 71/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
Table 22. Typical and maximum current consumption in run mode, code with data processing
(ART accelerator enabled except prefetch) running from flash memory- VDD = 1.7 V
```
```
Symbol Parameter Conditions
```
```
fHCLK
(MHz) Typ
```
```
Max(1)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA=

##### 125 °C

```
IDD Supply current
in Run mode
```
```
External clock, PLL
ON(2),
all peripherals
enabled(3)(4)
```
##### 100 20.4 21.8 22.1 22.8 23.8

```
mA
```
##### 84 16.5 17.6 17.8 18.6 19.6

##### 64 11.4 12.3 12.5 13.1 14.1

##### 50 9.0 9.7 10.0 10.6 11.6

##### 20 4.6 5.0 5.3 6.0 7.0

```
HSI, PLL OFF(2), all
peripherals enabled(3)
```
##### 16 2.9 3.2 3.6 4.3 5.3

##### 1 0.7 0.8 1.3 1.9 2.9

```
External clock, PLL ON(2)
all peripherals disabled(3)
```
##### 100 11.2 12.2 12.4 13.2 14.2

##### 84 9.1 9.9 10.1 10.9 11.9

##### 64 6.4 7.0 7.3 7.9 8.9

##### 50 5.1 5.6 5.9 6.6 7.6

##### 20 2.6 3.0 3.3 4.0 5.0

```
HSI, PLL OFF(2), all
peripherals disabled(3)
```
##### 16 1.8 2.0 2.4 3.0 4.0

##### 1 0.6 0.7 1.2 1.9 2.9

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
    while the ADC is ON (ADON bit is set in the ADC_CR2 register).
4. When the ADC is ON (ADON bit set in the ADC_CR2), add an additional power consumption of 1.6mA per ADC for the
    analog part.


**Electrical characteristics STM32F411xC STM32F411xE**

72/151 DS10314 Rev 8

```
Table 23. Typical and maximum current consumption in run mode, code with data processing
(ART accelerator enabled except prefetch) running from flash memory - VDD = 3.6 V
```
```
Symbol Parameter Conditions
```
```
fHCLK
(MHz) Typ
```
```
Max(1)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

```
IDD Supply current
in Run mode
```
```
External clock, PLL
ON(2),
all peripherals
enabled(3)(4)
```
##### 100 20.7 22.2 22.5 23.2 24.4

```
mA
```
##### 84 16.8 18.0 18.3 19.0 20.1

##### 64 11.8 12.7 12.9 13.6 14.6

##### 50 9.3 10.2 10.4 11.1 12.0

##### 20 4.8 5.5 5.8 6.5 7.4

```
HSI, PLL OFF(2), all
peripherals enabled(3)
```
##### 16 3.0 3.3 3.8 4.5 5.4

##### 1 0.7 1.0 1.4 2.1 3.0

```
External clock, PLL ON(2)
all peripherals disabled(3)
```
##### 100 11.6 12.6 12.9 13.6 14.8

##### 84 9.7 10.2(5) 11.1 11.3 12.5

##### 64 6.7 7.4 7.7 8.3 9.4

##### 50 5.4 6.0 6.3 7.0 8.0

##### 20 2.9 3.4 3.7 4.4 5.4

```
HSI, PLL OFF(2), all
peripherals disabled(3)
```
##### 16 1.9 2.2 2.6 3.3 4.3

##### 1 0.7 0.9 1.3 2.1 3.1

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
    while the ADC is ON (ADON bit is set in the ADC_CR2 register).
4. When the ADC is ON (ADON bit set in the ADC_CR2), add an additional power consumption of 1.6mA per ADC for the
    analog part.
5. Guaranteed by test in production.


```
DS10314 Rev 8 73/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
Table 24. Typical and maximum current consumption in run mode, code with data processing
(ART accelerator disabled) running from flash memory - VDD = 3.6 V
```
```
Symbol Parameter Conditions
```
```
fHCLK
(MHz) Typ
```
```
Max(1)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

```
IDD Supply current
in Run mode
```
```
External clock, PLL
ON(2),
all peripherals
enabled(3)(4)
```
##### 100 29.5 31.5 32.3 33.3 34.7

```
mA
```
##### 84 25.5 27.1 27.9 28.9 30.2

##### 64 18.6 19.8 20.4 21.2 22.4

##### 50 15.2 16.4 16.9 17.7 18.7

##### 20 7.6 8.4 8.8 9.5 10.5

```
HSI, PLL OFF(2), all
peripherals enabled(3)
```
##### 16 4.8 5.2 5.7 6.5 7.5

##### 1 0.9 1.3 1.6 2.4 3.4

```
External clock, PLL ON(2)
all peripherals disabled(3)
```
##### 100 20.4 21.8 22.7 23.8 25.1

##### 84 18.4 19.2(5) 20.9 21.1 22.4

##### 64 13.5 14.5 15.2 15.9 17.2

##### 50 11.3 12.2 12.8 13.6 14.7

##### 20 5.6 6.4 6.7 7.4 8.5

```
HSI, PLL OFF(2), all
peripherals disabled(3)
```
##### 16 3.6 4.1 4.5 5.2 6.3

##### 1 0.9 1.2 1.6 2.3 3.4

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
    while the ADC is ON (ADON bit is set in the ADC_CR2 register).
4. When the ADC is ON (ADON bit set in the ADC_CR2), add an additional power consumption of 1.6mA per ADC for the
    analog part.
5. Guaranteed by test in production.


**Electrical characteristics STM32F411xC STM32F411xE**

74/151 DS10314 Rev 8

```
Table 25. Typical and maximum current consumption in run mode, code with data processing
(ART accelerator enabled with prefetch) running from flash memory - VDD = 3.6 V
```
```
Symbol Parameter Conditions
```
```
fHCLK
(MHz) Typ
```
```
Max(1)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

```
IDD Supply current
in Run mode
```
```
External clock, PLL
ON(2),
all peripherals
enabled(3)(4)
```
##### 100 31.7 33.6 34.5 35.5 37.0

```
mA
```
##### 84 26.9 28.6 29.4 30.3 31.6

##### 64 19.6 20.9 21.5 22.3 23.5

##### 50 15.6 16.7 17.2 18.0 19.1

##### 20 7.6 8.4 8.8 9.5 10.6

```
HSI, PLL OFF(2), all
peripherals enabled(3)
```
##### 16 5.1 5.6 6.1 6.8 7.9

##### 1 1.0 1.3 1.7 2.3 3.4

```
External clock, PLL ON(2)
all peripherals disabled(3)
```
##### 100 22.5 24.2 24.9 26.0 27.3

##### 84 19.5 21.1(5) 21.8 22.8 24.1

##### 64 14.5 15.7 16.3 17.1 18.3

##### 50 11.7 12.7 13.2 14.0 15.1

##### 20 5.6 6.4 6.8 7.4 8.5

```
HSI, PLL OFF(2), all
peripherals disabled(3)
```
##### 16 4.0 4.5 4.9 5.6 6.7

##### 1 0.9 1.2 1.6 2.2 3.3

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting
3. Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
    while the ADC is ON (ADON bit is set in the ADC_CR2 register).
4. When the ADC is ON (ADON bit set in the ADC_CR2), add an additional power consumption of 1.6mA per ADC for the
    analog part.
5. Guaranteed by test in production.


```
DS10314 Rev 8 75/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Table 26. Typical and maximum current consumption in Sleep mode - VDD = 3.6 V

```
Symbol Parameter Conditions
```
```
fHCLK
(MHz) Typ
```
```
Max(1)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

```
IDD Supply current
in Sleep mode
```
```
External clock, PLL
ON(2),
all peripherals
enabled(3)(4)
```
##### 100 12.2 13.2 13.4 14.1 15.3

```
mA
```
##### 84 9.8 10.6 10.9 11.6 12.8

##### 64 6.9 7.4 7.7 8.3 9.5

##### 50 5.4 5.9 6.2 6.8 8.0

##### 20 2.8 3.2 3.5 4.1 5.3

```
HSI, PLL OFF(2), all
peripherals enabled(3)
```
##### 16 1.3 1.7 2.2 2.8 4.0

##### 1 0.4 0.5 0.9 1.6 2.8

```
External clock, PLL ON(2)
all peripherals disabled(3)
```
##### 100 3.0 3.6 3.9 4.5 5.7

##### 84 2.5 3.0 3.2 3.9 5.1

##### 64 1.9 2.2 2.5 3.0 4.2

##### 50 1.6 1.9 2.1 2.7 3.9

##### 20 1.1 1.4 1.7 2.3 3.5

```
HSI, PLL OFF(2), all
peripherals disabled(3)
```
##### 16 0.4 0.5 0.9 1.6 2.8

##### 1 0.3 0.4 0.8 1.5 2.7

1. Evaluated by characterization - Not tested in production.
2. Refer to _Table 41_ and RM0383 for the possible PLL VCO setting.
3. Add an additional power consumption of 1.6 mA per ADC for the analog part. In applications, this consumption occurs only
    while the ADC is ON (ADON bit is set in the ADC_CR2 register).
4. When the ADC is ON (ADON bit set in the ADC_CR2), add an additional power consumption of 1.6mA per ADC for the
    analog part.

## Table 27. Typical and maximum current consumptions in Stop mode - VDD = 1.7 V

```
Symbol Conditions Parameter
```
```
Typ(1) Max(1)
Unit
TA =
25 °C
```
##### TA =

##### 25 °C

##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

##### IDD_STOP

```
Flash in Stop mode, all
oscillators OFF, no
independent watchdog
```
```
Main regulator usage 112 142 (2) 400 710 1200 (2)
```
```
μA
```
```
Low power regulator usage 42.6 67 (2) 300 580 1044 (2)
```
```
Flash in Deep power
down mode, all
oscillators OFF, no
independent watchdog
```
```
Main regulator usage 75 99 (2) 310 580 993 (2)
Low power regulator usage 13.6 37 (2) 265 550 1007 (2)
Low power low voltage regulator
usage
```
##### 9 28 (2) 230 500 910 (2)

1. Evaluated by characterization - Not tested in production.
2. Guaranteed by test in production.


**Electrical characteristics STM32F411xC STM32F411xE**

76/151 DS10314 Rev 8

## Table 28. Typical and maximum current consumption in Stop mode - VDD=3.6 V

```
Symbol Conditions Parameter
```
```
Typ Max(1)
Unit
TA =
25 °C
```
##### TA =

##### 25 °C

##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

##### IDD_STOP

```
Flash in Stop mode, all
oscillators OFF, no
independent watchdog
```
```
Main regulator usage 113.7 145 (2) 410 720 1217 (2)
```
```
μA
```
```
Low power regulator usage 43.1 68 (2) 310 600 1073 (2)
```
```
Flash in Deep power
down mode, all
oscillators OFF, no
independent watchdog
```
```
Main regulator usage 76.2 105 (2) 320 600 1019 (2)
Low power regulator usage 14 38 (2) 275 560 1025 (2)
Low power low voltage regulator
usage
```
##### 10 30 (2) 235 510 928 (2)

1. Evaluated by characterization - Not tested in production.
2. Guaranteed by test in production.

## Table 29. Typical and maximum current consumption in Standby mode - VDD= 1.7 V

```
Symbol Parameter Conditions
```
```
Typ(1) Max(2)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 25 °C

##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

```
IDD_STBY Supply current
in Standby mode
```
```
Low-speed oscillator (LSE) and RTC ON 2.4 4 12 25 50
μA
RTC and LSE OFF 1.8 3 (3) 11 24 49 (3)
```
1. When the PDR is OFF (internal reset is OFF), the typical current consumption is reduced by 1.2 μA.
2. Evaluated by characterization - Not tested in production.
3. Guaranteed by test in production.

## Table 30. Typical and maximum current consumption in Standby mode - VDD= 3.6 V

```
Symbol Parameter Conditions
```
```
Typ(1) Max(2)
```
```
T Unit
A =
25 °C
```
##### TA =

##### 25 °C

##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

##### IDD_STBY

```
Supply current
in Standby
mode
```
```
Low-speed oscillator (LSE) and RTC ON 2.8 5 14 29 59
μA
RTC and LSE OFF 2.1 4 (3) 13.5 28 58 (3)
```
1. When the PDR is OFF (internal reset is OFF), the typical current consumption is reduced by 1.2 μA.
2. Evaluated by characterization - Not tested in production.
3. Guaranteed by test in production.


```
DS10314 Rev 8 77/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 20. Typical VBAT current consumption (LSE in low-drive mode and RTC ON).

## Table 31. Typical and maximum current consumptions in VBAT mode.

```
Symbol Parameter Conditions(1)
```
```
Typ Max(2)
```
```
TA = 25 °C Unit
```
##### TA =

##### 85 °C

##### TA =

##### 105 °C

##### TA =

##### 125 °C

##### VBAT =

##### 1.7 V

##### VBAT=

##### 2.4 V

##### VBAT =

##### 3.3 V VBAT = 3.6 V

##### IDD_VBAT

```
Backup
domain
supply
current
```
```
Low-speed oscillator (LSE in low-
drive mode) and RTC ON 0.7 0.8 1.0^35 6.8
Low-speed oscillator (LSE in high- μA
drive mode) and RTC ON 1.5 1.6 1.9 3.8 5.8 8.6
RTC and LSE OFF 0.1 0.1 0.1 2 4 5.8
```
1. Crystal used: Abracon ABS07-120-32.768 kHz-T with a CL of 6 pF for typical values.
2. Evaluated by characterization - Not tested in production.

```
MS30490V1
```
##### 0

##### 0.5

##### 1

##### 1.5

##### 2

##### 2.5

##### 3

##### 0°C 25°C55°C85°C105°C

```
IDD_VBAT (μA)
```
```
Temperature
```
##### 1.65V

##### 1.7V

##### 1.8V

##### 2V

##### 2.4V

##### 2.7V

##### 3V

##### 3.3V

##### 3.6V


**Electrical characteristics STM32F411xC STM32F411xE**

78/151 DS10314 Rev 8

**I/O system current consumption**

```
The current consumption of the I/O system has two components: static and dynamic.
```
```
I/O static current consumption
```
```
All the I/Os used as inputs with pull-up generate current consumption when the pin is
externally held low. The value of this current consumption can be simply computed by using
the pull-up/pull-down resistors values given in Table 53: I/O static characteristics.
```
```
For the output pins, any external pull-down or external load must also be considered to
estimate the current consumption.
```
```
Additional I/O current consumption is due to I/Os configured as inputs if an intermediate
voltage level is externally applied. This current consumption is caused by the input Schmitt
trigger circuits used to discriminate the input value. Unless this specific configuration is
required by the application, this supply current consumption can be avoided by configuring
these I/Os in analog mode. This is notably the case of ADC input pins which should be
configured as analog inputs.
```
**Caution:** Any floating input pin can also settle to an intermediate voltage level or switch inadvertently,
as a result of external electromagnetic noise. To avoid current consumption related to
floating pins, they must either be configured in analog mode, or forced internally to a definite
digital value. This can be done either by using pull-up/down resistors or by configuring the
pins in output mode.

```
I/O dynamic current consumption
```
```
In addition to the internal peripheral current consumption (see Table 33: Peripheral current
consumption ), the I/Os used by an application also contribute to the current consumption.
When an I/O pin switches, it uses the current from the MCU supply voltage to supply the I/O
pin circuitry and to charge/discharge the capacitive load (internal or external) connected to
the pin:
```
```
where
ISW is the current sunk by a switching I/O to charge/discharge the capacitive load
VDD is the MCU supply voltage
fSW is the I/O switching frequency
C is the total capacitance seen by the I/O pin: C = CINT+ CEXT
```
```
The test pin is configured in push-pull output mode and is toggled by software at a fixed
frequency.
```
#### ISW= VDD×fSW×C


```
DS10314 Rev 8 79/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Table 32. Switching output I/O current consumption

```
Symbol Parameter Conditions(1)
```
1. CS is the PCB board capacitance including the pad pin. CS = 7 pF (estimated value).

```
I/O toggling
frequency (fSW)
```
```
Typ Unit
```
##### IDDIO

```
I/O switching
current
```
##### VDD = 3.3 V

##### C = CINT

```
2 MHz 0.05
```
```
mA
```
```
8 MHz 0.15
```
```
25 MHz 0.45
```
```
50 MHz 0.85
```
```
60 MHz 1.00
```
```
84 MHz 1.40
```
```
90 MHz 1.67
```
##### VDD = 3.3 V

```
CEXT = 0 pF
C = CINT + CEXT + CS
```
```
2 MHz 0.10
```
```
8 MHz 0.35
```
```
25 MHz 1.05
```
```
50 MHz 2.20
```
```
60 MHz 2.40
```
```
84 MHz 3.55
```
```
90 MHz 4.23
```
##### VDD = 3.3 V

```
CEXT =10 pF
C = CINT + CEXT + CS
```
```
2 MHz 0.20
```
```
8 MHz 0.65
```
```
25 MHz 1.85
```
```
50 MHz 2.45
```
```
60 MHz 4.70
```
```
84 MHz 8.80
```
```
90 MHz 10.47
```
##### VDD = 3.3 V

```
CEXT = 22 pF
C = CINT + CEXT + CS
```
```
2 MHz 0.25
```
```
8 MHz 1.00
```
```
25 MHz 3.45
```
```
50 MHz 7.15
```
```
60 MHz 11.55
```
##### VDD = 3.3 V

```
CEXT = 33 pF
C = CINT + CEXT + CS
```
```
2 MHz 0.32
```
```
8 MHz 1.27
```
```
25 MHz 3.88
```
```
50 MHz 12.34
```

**Electrical characteristics STM32F411xC STM32F411xE**

80/151 DS10314 Rev 8

**On-chip peripheral current consumption**

```
The MCU is placed under the following conditions:
```
- At startup, all I/O pins are in analog input configuration.
- All peripherals are disabled unless otherwise mentioned.
- The ART accelerator is ON.
- Voltage Scale 2 mode selected, internal digital voltage V12 = 1.26 V.
- HCLK is the system clock at 84 MHz. fPCLK1 = fHCLK/2, and fPCLK2 = fHCLK.
    The given value is calculated by measuring the difference of current consumption
    - with all peripherals clocked off
    - with only one peripheral clocked on
- Ambient operating temperature is 25 °C and VDD=3.3 V.

## Table 33. Peripheral current consumption

```
Peripheral IDD (Typ) Unit
```
##### AHB1

```
(up to 100 MHz)
```
##### GPIOA 1.55

```
μA/MHz
```
##### GPIOB 1.55

##### GPIOC 1.55

##### GPIOD 1.55

##### GPIOE 1.55

##### GPIOH 1.55

##### CRC 0.36

##### DMA1(1) 14.96

##### DMA1(2) 1.54N+2.66

##### DMA2(1) 14.96

##### DMA2(2) 1.54N+2.66

##### APB1

```
(up to 50 MHz)
```
##### TIM2 11.19

```
μA/MHz
```
##### TIM3 8.57

##### TIM4 8.33

##### TIM5 11.19

##### PWR 0.71

##### USART2 3.33

##### I2C1/2/3 3.10

##### SPI2(3) 2.62

##### SPI3(3) 2.86

##### I2S2 1.90

##### I2S3 1.67

##### WWDG 0.71


```
DS10314 Rev 8 81/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
##### APB2

```
(up to 100 MHz)
```
##### TIM1 5.71

```
μA/MHz
```
##### TIM9 2.86

##### TIM10 1.79

##### TIM11 2.02

##### OTG_FS 23.93

##### ADC1(4) 2.98

##### SPI1 1.19

##### USART1 3.10

##### USART6 2.86

##### SDIO 5.95

##### SPI4 1.31

##### SYSCFG 0.71

1. Valid if all the DMA streams are activated (please refer to the reference manual RM0383).
2. For N DMA streams activated (up to 8 activated streams, refer to the reference manual RM0383).
3. I2SMOD bit set in SPI_I2SCFGR register, and then the I2SE bit set to enable I2S peripheral.
4. When the ADC is ON (ADON bit set in the ADC_CR2 register), add an additional power consumption of 1.6
    mA for the analog part.

```
Table 33. Peripheral current consumption (continued)
```
```
Peripheral IDD (Typ) Unit
```

**Electrical characteristics STM32F411xC STM32F411xE**

82/151 DS10314 Rev 8

#### 6.3.7 Wakeup time from low-power modes

```
The wakeup times given in Table 34 are measured starting from the wakeup event trigger up
to the first instruction executed by the CPU:
```
- For Stop or Sleep modes: the wakeup event is WFE.
- WKUP (PA0) pin is used to wakeup from Standby, Stop and Sleep modes.

## Figure 21. Low-power mode wakeup

```
All timings are derived from tests performed under ambient temperature and VDD=3.3 V.
```
```
Regulator
ramp-up
```
```
HSI restart Flash stop exit
```
```
CPU restart
```
```
Wakeup from Stop mode,
main regulator
```
```
Regulator
ramp-up
```
```
HSI restart Flash Deep Pd recovery
```
```
CPU restart
```
```
Wakeup from Stop mode,
main regulator,
flash in Deep power down mode
```
```
Regulator
ramp-up
```
```
HSI restart Flash stop exit
```
```
CPU restart
```
```
Wakeup from Stop,
regulator in low power mode
```
```
Regulator
ramp-up
```
```
HSI restart
```
```
CPU restart
```
```
Wakeup from Stop,
regulator in low power mode,
flash in Deep power down mode
```
```
Regulator
restart
```
```
HSI restart
```
```
CPU restart
```
```
Wakeup from Standby mode
```
```
CPU restart
```
```
Wakeup from Sleep and
Flash in Deep power down
```
```
MS35542V1
```
```
Flash Deep Pd recovery
```
```
Option bytes are not reloaded
```
```
Option bytes are not reloaded
```
```
Flash Deep Pd recovery Option bytes loading
```
```
Flash Deep Pd recovery
```
```
Option bytes are not reloaded
```
```
Option bytes are not reloaded
```
```
Regulator
OFF
```
```
Regulator Option bytes are not reloaded
ON
```

```
DS10314 Rev 8 83/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.8 External clock source characteristics

**High-speed external user clock generated from an external source**

```
In bypass mode the HSE oscillator is switched off and the input pin is a standard I/O. The
external clock signal has to respect the Table 53. However, the recommended clock input
waveform is shown in Figure 22.
```
```
The characteristics given in Table 35 result from tests performed using an high-speed
external clock source, and under ambient temperature and supply voltage conditions
summarized in Table 14.
```
## Table 34. Low-power mode wakeup timings(1).

```
Symbol Parameter Min(1) Typ(1) Max(1) Unit
```
```
tWUSLEEP(2) Wakeup from Sleep mode - 4 6
```
##### CPU

```
clock
cycle
```
```
tWUSTOP(2)
```
```
Wakeup from Stop mode, usage of main regulator - 13.5 14.5
```
```
μs
```
```
Wakeup from Stop mode, usage of main regulator, flash
memory in Deep power down mode
```
##### -105111

```
Wakeup from Stop mode, regulator in low power mode - 21 33
```
```
Wakeup from Stop mode, regulator in low power mode,
flash memory in Deep power down mode
```
##### - 113 130

```
tWUSTDBY(2)(3) Wakeup from Standby mode - 314 407 μs
```
```
tWUFLASH
```
```
Wakeup of Flash from Flash_Stop mode - - 8
μs
Wakeup of Flash from Flash Deep power down mode - - 100
```
1. Evaluated by characterization - Not tested in production.
2. The wakeup times are measured from the wakeup event to the point in which the application code reads the first instruction.
3. tWUSTDBY maximum value is given at –40 °C.

## Table 35. High-speed external user clock characteristics.

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fHSE_ext External user clock source frequency(1)
```
##### -

```
1-50MHz
```
```
VHSEH OSC_IN input pin high level voltage 0.7VDD -VDD
V
VHSEL OSC_IN input pin low level voltage VSS -0.3VDD
```
```
tw(HSEH)
tw(HSEL)
```
```
OSC_IN high or low time(1) 5--
ns
tr(HSE)
tf(HSE)
```
```
OSC_IN rise or fall time(1) --10
```
```
Cin(HSE) OSC_IN input capacitance(1) --5-pF
```
```
DuCy(HSE) Duty cycle - 45 - 55 %
```
```
IL OSC_IN Input leakage current VSS ≤ VIN ≤ VDD --±1μA
```
1. Guaranteed by design - Not tested in production.


**Electrical characteristics STM32F411xC STM32F411xE**

84/151 DS10314 Rev 8

## Figure 22. High-speed external clock source AC timing diagram

**Low-speed external user clock generated from an external source**

```
In bypass mode the LSE oscillator is switched off and the input pin is a standard I/O. The
external clock signal has to respect the Table 53. However, the recommended clock input
waveform is shown in Figure 23.
```
```
The characteristics given in Table 36 result from tests performed using an low-speed
external clock source, and under ambient temperature and supply voltage conditions
summarized in Table 14.
```
```
MSv42627V1
```
```
VHSEH
```
```
tf(HSE)
```
```
90%
10%
```
```
THSE
```
```
tr(HSE) t
```
```
VHSEL
```
```
tw(HSEH)
```
```
tw(HSEL)
```
## Table 36. Low-speed external user clock characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fLSE_ext User External clock source
frequency(1)
```
##### -

- 32.768 1000 kHz

```
VLSEH OSC32_IN input pin high level
voltage
```
##### 0.7VDD -VDD

##### V

```
VLSEL OSC32_IN input pin low level voltage VSS -0.3VDD
```
```
tw(LSEH)
tw(LSEL)
```
```
OSC32_IN high or low time(1) 450 - -
ns
tr(LSE)
tf(LSE)
```
```
OSC32_IN rise or fall time(1) --50
```
```
Cin(LSE) OSC32_IN input capacitance(1) --5-pF
```
```
DuCy(LSE) Duty cycle - 30 - 70 %
```
```
IL OSC32_IN Input leakage current VSS ≤ VIN ≤ VDD --±1μA
```
1. Guaranteed by design - Not tested in production.


```
DS10314 Rev 8 85/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 23. Low-speed external clock source AC timing diagram

**High-speed external clock generated from a crystal/ceramic resonator**

```
The high-speed external (HSE) clock can be supplied with a 4 to 26 MHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 37. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
```
```
For CL1 and CL2, it is recommended to use high-quality external ceramic capacitors in the
5 pF to 25 pF range (Typ.), designed for high-frequency applications, and selected to match
the requirements of the crystal or resonator (see Figure 24 ). CL1 and CL2 are usually the
same size. The crystal manufacturer typically specifies a load capacitance which is the
series combination of CL1 and CL2. PCB and MCU pin capacitance must be included (10 pF
can be used as a rough estimate of the combined pin and board capacitance) when sizing
CL1 and CL2.
```
_Note: For information on selecting the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website [http://www.st.com.](http://www.st.com.)_

## Table 37. HSE 4-26 MHz oscillator characteristics.

1. Guaranteed by design - Not tested in production.

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fOSC_IN Oscillator frequency 4 - 26 MHz
```
```
RF Feedback resistor - 200 - kΩ
```
```
IDD HSE current consumption
```
##### VDD=3.3 V,

##### ESR= 30 Ω,

```
CL=5 pF @25 MHz
```
##### -450-

```
μA
VDD=3.3 V,
ESR= 30 Ω,
CL=10 pF @25 MHz
```
##### -530-

```
Gm_crit_max Maximum critical crystal gm Startup - - 1 mA/V
tSU(HSE)(2)
```
2. tSU(HSE) is the startup time measured from the moment it is enabled (by software) to a stabilized 8 MHz
    oscillation is reached. This value is measured for a standard crystal resonator and it can vary significantly
    with the crystal manufacturer

```
Startup time VDD is stabilized - 2 - ms
```
```
MSv42626V1
```
```
VLSEH
```
```
tf(LSE)
```
```
90%
10%
```
```
TLSE
```
```
tr(LSE) t
```
```
VLSEL
```
```
tw(LSEH)
```
```
tw(LSEL)
```

**Electrical characteristics STM32F411xC STM32F411xE**

86/151 DS10314 Rev 8

## Figure 24. Typical application with an 8 MHz crystal

1. REXT value depends on the crystal characteristics.

**Low-speed external clock generated from a crystal/ceramic resonator**

```
The low-speed external (LSE) clock can be supplied with a 32.768 kHz crystal/ceramic
resonator oscillator. All the information given in this paragraph are based on
characterization results obtained with typical external components specified in Table 38. In
the application, the resonator and the load capacitors have to be placed as close as
possible to the oscillator pins in order to minimize output distortion and startup stabilization
time. Refer to the crystal resonator manufacturer for more details on the resonator
characteristics (frequency, package, accuracy).
```
```
The LSE high-power mode allows to cover a wider range of possible crystals but with a cost
of higher power consumption.
```
_Note: For information on selecting the crystal, refer to the application note AN2867 “Oscillator
design guide for ST microcontrollers” available from the ST website [http://www.st.com.](http://www.st.com.)
For information about the LSE high-power mode, refer to the reference manual RM0383._

## Table 38. LSE oscillator characteristics (fLSE = 32.768 kHz)

1. Guaranteed by design - Not tested in production.

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
RF Feedback resistor - - 18.4 - MΩ
```
```
IDD LSE current
consumption
```
```
Low-power mode
(default) --1μA
```
```
High-drive mode - - 3
```
```
Gm_crit_max
```
```
Maximum critical crystal
gm
```
```
Startup, low-power mode - - 0.56
μA/V
Startup, high-drive mode - - 1.50
```
```
tSU(LSE)(2)
```
2. tSU(LSE) is the startup time measured from the moment it is enabled (by software) to a stabilized
    32.768 kHz oscillation is reached. This value is guaranteed by characterization. It is measured for a
    standard crystal resonator and it can vary significantly with the crystal manufacturer.

```
startup time VDD is stabilized - 2 - s
```
```
ai17530
```
```
OSC_OU T
```
```
OSC_IN fHSE
```
```
CL1
```
```
RF
```
```
STM32F
```
```
8 MHz
resonator
```
```
Resonator with
integrated capacitors
```
```
Bias
controlled
gain
```
```
CL2 REXT(1)^
```

```
DS10314 Rev 8 87/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 25. Typical application with a 32.768 kHz crystal

#### 6.3.9 Internal clock source characteristics

```
The parameters given in Table 39 and Table 40 are derived from tests performed under
ambient temperature and VDD supply voltage conditions summarized in Table 14.
```
**High-speed internal (HSI) RC oscillator**

```
L
```
```
ai17531
```
```
OSC32_OU T
```
```
OSC32_IN fLSE
```
```
CL1
```
```
RF
```
```
STM32F
```
```
32.768 kH z
resonator
```
```
Resonator with
integrated capacitors
```
```
Bias
controlled
gain
```
```
CL2
```
## Table 39. HSI oscillator characteristics

1. VDD = 3.3 V, TA = - 40 to 125 °C unless otherwise specified.

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fHSI Frequency - - 16 - MHz
```
##### ACCHSI

```
Accuracy of the HSI
oscillator
```
```
User-trimmed with the RCC_CR
register(2)
```
2. Guaranteed by design - Not tested in production.

##### --1%

```
Factory-
calibrated
```
```
TA = - 40 to 125 °C(3)
```
3. Evaluated by characterization - Not tested in production.

##### - 8 - 5.5

```
TA = - 40 to 105 °C(3) - 8 - 4.5 %
```
```
TA = - 10 to 85 °C(3) - 4 - 4 %
```
```
TA = 25 °C(4)
```
4. Factory calibrated non-soldered parts.

##### - 1 - 1 %

```
tsu(HSI)(2)
```
```
HSI oscillator
startup time -2.24 μs
```
##### IDD(HSI)(2)

```
HSI oscillator
power consumption -6080μA
```

**Electrical characteristics STM32F411xC STM32F411xE**

88/151 DS10314 Rev 8

## Figure 26. ACCHSI versus temperature

1. Evaluated by characterization - Not tested in production.

**Low-speed internal (LSI) RC oscillator**

## Table 40. LSI oscillator characteristics

1. VDD = 3 V, TA = –40 to 125 °C unless otherwise specified.

```
Symbol Parameter Min Typ Max Unit
```
```
fLSI(2)
```
2. Evaluated by characterization - Not tested in production.

```
Frequency 17 32 47 kHz
```
```
tsu(LSI)(3)
```
3. Guaranteed by design - Not tested in production.

```
LSI oscillator startup time - 15 40 μs
```
```
IDD(LSI)(3) LSI oscillator power consumption - 0.4 0.6 μA
```
```
MS30492V1
```
```
-0. 08
```
```
-0. 06
```
```
-0. 04
```
```
-0. 02
```
```
0
```
```
0.02
```
```
0.04
```
```
0.06
```
```
-40 0 25 5 8 1 05 1 25
```
```
Min
Max
Typical
```
```
TA (°C)
ACC
```
```
HSI
```

```
DS10314 Rev 8 89/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 27. ACCLSI versus temperature

#### 6.3.10 PLL characteristics

```
The parameters given in Table 41 and Table 42 are derived from tests performed under
temperature and VDD supply voltage conditions summarized in Table 14.
```
```
MS19013V1
```
```
-40
```
```
-30
```
```
-20
```
```
-10
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
-45-35-25-15-5 5 152535455565758595105
```
```
Normalized deviati on (%)
```
```
Te m p e r at u r e ( °C)
```
```
max
avg
min
```
## Table 41. Main PLL characteristics.

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fPLL_IN PLL input clock(1) 0.95(2) 12.10MHz
```
```
fPLL_OUT PLL multiplier output clock 24 - 100 MHz
```
```
fPLL48_OUT
```
```
48 MHz PLL multiplier output
clock -48 75MHz
```
```
fVCO_OUT PLL VCO output 100 - 432 MHz
```
```
tLOCK PLL lock time
```
```
VCO freq = 100 MHz 75 - 200
μs
VCO freq = 432 MHz 100 - 300
```
```
Jitter(3)
```
```
Cycle-to-cycle jitter
```
```
System clock
100 MHz
```
##### RMS - 25 -

```
ps
```
```
peak
to
peak
```
##### - ± 150 -

```
Period Jitter
```
##### RMS - 15 -

```
peak
to
peak
```
##### - ± 200 -


**Electrical characteristics STM32F411xC STM32F411xE**

90/151 DS10314 Rev 8

```
IDD(PLL)(4) PLL power consumption on VDD
```
```
VCO freq = 100 MHz
VCO freq = 432 MHz
```
##### 0.15

##### 0.45

##### -

##### 0.40

##### 0.75

```
mA
IDDA(PLL)(4)
```
```
PLL power consumption on
VDDA
```
```
VCO freq = 100 MHz
VCO freq = 432 MHz
```
##### 0.30

##### 0.55

##### -

##### 0.40

##### 0.85

1. Take care of using the appropriate division factor M to obtain the specified PLL input clock values. The M factor is shared
    between PLL and PLLI2S.
2. Guaranteed by design - Not tested in production.
3. The use of two PLLs in parallel could degraded the Jitter up to +30%.
4. Evaluated by characterization - Not tested in production.

```
Table 41. Main PLL characteristics (continued)
```
```
Symbol Parameter Conditions Min Typ Max Unit
```
## Table 42. PLLI2S (audio PLL) characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fPLLI2S_IN PLLI2S input clock(1) -0.95(2) 12.10
```
```
fPLLI2S_OUT PLLI2S multiplier output clock - - - 216 MHz
```
```
fVCO_OUT PLLI2S VCO output - 100 - 432
```
```
tLOCK PLLI2S lock time
```
```
VCO freq = 100 MHz 75 - 200
μs
VCO freq = 432 MHz 100 - 300
```
```
Jitter(3)
```
```
Master I2S clock jitter
```
```
Cycle to cycle at
12.288 MHz on
48 kHz period,
N=432, R=5
```
##### RMS - 90 -

```
peak
to
peak
```
##### - ± 280 -

```
ps
```
```
Average frequency of
12.288 MHz
N = 432, R = 5
on 1000 samples
```
##### -90 -

```
WS I2S clock jitter
```
```
Cycle to cycle at 48 KHz
on 1000 samples
```
##### - 400 -

##### IDD(PLLI2S)(4)

```
PLLI2S power consumption on
VDD
```
```
VCO freq = 100 MHz
VCO freq = 432 MHz
```
##### 0.15

##### 0.45

##### -

##### 0.40

##### 0.75

```
mA
IDDA(PLLI2S)(4)
```
```
PLLI2S power consumption on
VDDA
```
```
VCO freq = 100 MHz
VCO freq = 432 MHz
```
##### 0.30

##### 0.55

##### -

##### 0.40

##### 0.85

1. Take care of using the appropriate division factor M to have the specified PLL input clock values.
2. Guaranteed by design - Not tested in production.
3. Value given with main PLL running.
4. Evaluated by characterization - Not tested in production.


```
DS10314 Rev 8 91/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.11 PLL spread spectrum clock generation (SSCG) characteristics

```
The spread spectrum clock generation (SSCG) feature allows to reduce electromagnetic
interferences (see Table 49: EMI characteristics for LQFP100 ). It is available only on the
main PLL.
```
```
Equation 1
```
```
The frequency modulation period (MODEPER) is given by the equation below:
```
```
fPLL_IN and fMod must be expressed in Hz.
```
```
As an example:
```
```
If fPLL_IN = 1 MHz, and fMOD = 1 kHz, the modulation depth (MODEPER) is given by
equation 1:
```
```
Equation 2
```
```
Equation 2 allows to calculate the increment step (INCSTEP):
```
```
fVCO_OUT must be expressed in MHz.
```
```
With a modulation depth (md) = ±2 % (4 % peak to peak), and PLLN = 240 (in MHz):
```
```
An amplitude quantization error may be generated because the linear modulation profile is
obtained by taking the quantized values (rounded to the nearest integer) of MODPER and
INCSTEP. As a result, the achieved modulation depth is quantized. The percentage
quantized modulation depth is given by the following formula:
```
```
As a result:
```
## Table 43. SSCG parameter constraints

```
Symbol Parameter Min Typ Max(1) Unit
```
```
fMod Modulation frequency - - 10 kHz
```
```
md Peak modulation depth 0.25 - 2 %
```
```
MODEPER * INCSTEP (Modulation period) * (Increment Step) - - 215 -1 -
```
1. Guaranteed by design - Not tested in production.

```
MODEPER=round f[]PLL_IN⁄ ()4f× Mod
```
```
MODEPER==round 10[]^6 ⁄ () 410 ×^3250
```
```
INCSTEP=round[]()() 215 – 1 ×md×PLLN⁄ () 100 × 5 ×MODEPER
```
```
INCSTEP==round[]()() 215 – 1 × 2 × 240 ⁄ () 100 × 5 × 250 126md(quantitazed)%
```
```
mdquantized%=()MODEPER×INCSTEP× 100 × 5 ⁄ ()() 215 – 1 ×PLLN
```
```
mdquantized%==() 250 × 126 × 100 × 5 ⁄ ()() 215 – 1 × 240 2.002%(peak)
```

**Electrical characteristics STM32F411xC STM32F411xE**

92/151 DS10314 Rev 8

```
Figure 28 and Figure 29 show the main PLL output clock waveforms in center spread and
down spread modes, where:
F0 is fPLL_OUT nominal.
Tmode is the modulation period.
md is the modulation depth.
```
## Figure 28. PLL output clock waveforms in center spread mode

## Figure 29. PLL output clock waveforms in down spread mode

#### 6.3.12 Memory characteristics

**Flash memory**

```
The characteristics are given at TA = - 40 to 125 °C unless otherwise specified.
```
```
The devices are shipped to customers with the flash memory erased.
```
```
Frequency (PLL_OUT)
```
```
Time
```
```
F0
```
```
tmode 2xtmode
```
```
md
```
```
ai17291
```
```
md
```
```
Frequency (PLL_OUT)
```
```
Time
```
```
F0
```
```
tmode 2xtmode
```
```
2xmd
```
```
ai17292b
```
## Table 44. Flash memory characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
IDD Supply current
```
```
Write / Erase 8-bit mode, VDD = 1.7 V - 5 -
```
```
Write / Erase 16-bit mode, VDD = 2.1 V - 8 - mA
```
```
Write / Erase 32-bit mode, VDD = 3.3 V - 12 -
```

```
DS10314 Rev 8 93/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Table 45. Flash memory programming

```
Symbol Parameter Conditions Min(1) Typ Max(1)
```
1. Evaluated by characterization - Not tested in production.

```
Unit
```
```
tprog Word programming time
```
```
Program/erase parallelism
(PSIZE) = x 8/16/32 -16100
```
```
(2)
```
2. The maximum programming time is measured after 100K erase operations.

```
μs
```
```
tERASE16KB Sector (16 KB) erase time
```
```
Program/erase parallelism
(PSIZE) = x 8 -^400800
```
```
ms
```
```
Program/erase parallelism
(PSIZE) = x 16 -^300600
```
```
Program/erase parallelism
(PSIZE) = x 32 -^250500
```
```
tERASE64KB Sector (64 KB) erase time
```
```
Program/erase parallelism
(PSIZE) = x 8 -^12002400
```
```
ms
```
```
Program/erase parallelism
(PSIZE) = x 16 -^7001400
```
```
Program/erase parallelism
(PSIZE) = x 32 -^5501100
```
```
tERASE128KB Sector (128 KB) erase time
```
```
Program/erase parallelism
(PSIZE) = x 8 -24
```
```
s
```
```
Program/erase parallelism
(PSIZE) = x 16 -1.32.6
```
```
Program/erase parallelism
(PSIZE) = x 32 -12
```
```
tME Mass erase time
```
```
Program/erase parallelism
(PSIZE) = x 8 -816
```
```
s
```
```
Program/erase parallelism
(PSIZE) = x 16 -5.511
```
```
Program/erase parallelism
(PSIZE) = x 32 -48
```
```
Vprog Programming voltage
```
```
32-bit program operation 2.7 - 3.6 V
```
```
16-bit program operation 2.1 - 3.6 V
```
```
8-bit program operation 1.7 - 3.6 V
```
## Table 46. Flash memory programming with VPP voltage

```
Symbol Parameter Conditions Min(1) Typ Max(1) Unit
```
```
tprog Double word programming
```
```
TA = 0 to +40 °C
VDD = 3.3 V
VPP = 8.5 V
```
- 16 100 (2) μs

```
tERASE16KB Sector (16 KB) erase time - 230 -
```
```
tERASE64KB Sector (64 KB) erase time - 490 - ms
```
```
tERASE128KB Sector (128 KB) erase time - 875 -
```
```
tME Mass erase time - 3.50 - s
```

**Electrical characteristics STM32F411xC STM32F411xE**

94/151 DS10314 Rev 8

## Table 47. Flash memory endurance and data retention

#### 6.3.13 EMC characteristics

```
Susceptibility tests are performed on a sample basis during device characterization.
```
**Functional EMS (electromagnetic susceptibility)**

```
While a simple application is executed on the device (toggling 2 LEDs through I/O ports).
the device is stressed by two electromagnetic events until a failure occurs. The failure is
indicated by the LEDs:
```
- **Electrostatic discharge (ESD)** (positive and negative) is applied to all device pins until
    a functional disturbance occurs. This test is compliant with the IEC 61000-4-2 standard.
- **FTB** : A burst of fast transient voltage (positive and negative) is applied to VDD and VSS
    through a 100 pF capacitor, until a functional disturbance occurs. This test is compliant
    with the IEC 61000-4-4 standard.

```
A device reset allows normal operations to be resumed.
```
```
The test results are given in Table 49. They are based on the EMS levels and classes
defined in application note AN1709.
```
```
Vprog Programming voltage 2.7 - 3.6 V
```
```
VPP VPP voltage range 7 - 9 V
```
##### IPP

```
Minimum current sunk on
the VPP pin^10 - - mA
```
```
tVPP(3)
```
```
Cumulative time during
which VPP is applied - -^1 hour
```
1. Guaranteed by design - Not tested in production.
2. The maximum programming time is measured after 100K erase operations.
3. VPP should only be connected during programming/erasing.

```
Symbol Parameter Conditions
```
```
Value
Unit
Min(1)
```
1. Evaluated by characterization - Not tested in production.

```
NEND Endurance
```
```
TA = - 40 to + 85 °C (temp. range 6)
TA = - 40 to + 105 °C (temp. range 7)
TA = - 40 to + 125 °C (temp. range 3)
```
```
10 kcycles
```
```
tRET Data retention
```
```
1 kcycle(2) at TA = 85 °C
```
2. Cycling performed over the whole temperature range.

##### 30

```
Years
```
```
1 kcycle(2) at TA = 105 °C 10
```
```
1 kcycle(2) at TA = 125 °C 3
```
```
10 kcycle(2) at TA = 55 °C 20
```
```
Table 46. Flash memory programming with VPP voltage (continued)
```
```
Symbol Parameter Conditions Min(1) Typ Max(1) Unit
```

```
DS10314 Rev 8 95/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
When the application is exposed to a noisy environment, it is recommended to avoid pin
exposition to disturbances. The pins showing a middle range robustness are: PA0, PA1,
PA2, on LQFP100 packages and PDR_ON on WLCSP49.
```
```
As a consequence, it is recommended to add a serial resistor (1 kΩ maximum) located as
close as possible to the MCU to the pins exposed to noise (connected to tracks longer than
50 mm on PCB).
```
**Designing hardened software to avoid noise problems**

```
EMC characterization and optimization are performed at component level with a typical
application environment and simplified MCU software. It should be noted that good EMC
performance is highly dependent on the user application and the software in particular.
```
```
Therefore it is recommended that the user applies EMC software optimization and
prequalification tests in relation with the EMC level requested for his application.
```
```
Software recommendations
```
```
The software flowchart must include the management of runaway conditions such as:
```
- Corrupted program counter
- Unexpected reset
- Critical Data corruption (control registers...)

```
Prequalification trials
```
```
Most of the common failures (unexpected reset and program counter corruption) can be
reproduced by manually forcing a low state on the NRST pin or the Oscillator pins for 1
second.
```
```
To complete these trials, ESD stress can be applied directly on the device, over the range of
specification values. When unexpected behavior is detected, the software can be hardened
to prevent unrecoverable errors occurring (see application note AN1015).
```
## Table 48. EMS characteristics for LQFP100 package

```
Symbol Parameter Conditions
```
```
Level/
Class
```
```
VFESD Voltage limits to be applied on any I/O pin
to induce a functional disturbance
```
##### VDD = 3.3 V, LQFP100, WLCSP49,

```
TA = +25 °C, fHCLK = 100 MHz,
conforms to IEC 61000-4-2
```
##### 2B

##### VEFTB

```
Fast transient voltage burst limits to be
applied through 100 pF on VDD and VSS
pins to induce a functional disturbance
```
##### VDD = 3.3 V, LQFP100, WLCSP49,

```
TA = +25 °C, fHCLK = 100 MHz,
conforms to IEC 61000-4-4
```
##### 4A


**Electrical characteristics STM32F411xC STM32F411xE**

96/151 DS10314 Rev 8

**Electromagnetic Interference (EMI)**

```
The electromagnetic field emitted by the device are monitored while a simple application,
executing EEMBC code, is running. This emission test is compliant with SAE IEC61967-2
standard which specifies the test board and the pin loading.
```
#### 6.3.14 Absolute maximum ratings (electrical sensitivity)

```
Based on three different tests (ESD, LU) using specific measurement methods, the device is
stressed in order to determine its performance in terms of electrical sensitivity.
```
**Electrostatic discharge (ESD)**

```
Electrostatic discharges (a positive then a negative pulse separated by 1 second) are
applied to the pins of each sample according to each pin combination. The sample size
depends on the number of supply pins in the device (3 parts × (n+1) supply pins). This test
conforms to the JESD22-A114/C101 standard.
```
## Table 49. EMI characteristics for LQFP100

```
Symbol Parameter Conditions Monitored
frequency band
```
```
Max vs.
[fHSE/fCPU]
Unit
```
```
8/84 MHz
```
```
SEMI Peak level
```
```
VDD = 3.6 V, TA = 25 °C, conforming to
IEC61967-2
```
```
0.1 to 30 MHz 19
```
```
30 to 130 MHz 17 dBμV
```
```
130 MHz to 1 GHz 12
```
```
SAE EMI Level 3.5 -
```
## Table 50. ESD absolute maximum ratings

```
Symbol Ratings Conditions Class
```
```
Maximum
value(1) Unit
```
##### VESD(HBM)

```
Electrostatic discharge
voltage (human body
model)
```
```
TA = +25 °C conforming to JESD22-A114 2 2000
```
##### V

##### VESD(CDM)

```
Electrostatic discharge
voltage (charge device
model)
```
```
TA = +25 °C conforming to
ANSI/ESD STM5.3.1
```
##### UFBGA100,

##### UFQFPN48^4500

##### WLCSP49 3 400

##### LQPF64,

##### LQFP100^3250

1. Evaluated by characterization - Not tested in production.


```
DS10314 Rev 8 97/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
**Static latchup**

```
Two complementary static tests are required on six parts to assess the latchup
performance:
```
- A supply overvoltage is applied to each power supply pin
- A current injection is applied to each input, output and configurable I/O pin

```
These tests are compliant with EIA/JESD 78A IC latchup standard.
```
#### 6.3.15 I/O current injection characteristics

```
As a general rule, current injection to the I/O pins, due to external voltage below VSS or
above VDD (for standard, 3 V-capable I/O pins) should be avoided during normal product
operation. However, in order to give an indication of the robustness of the microcontroller in
cases when abnormal injection accidentally happens, susceptibility tests are performed on a
sample basis during device characterization.
```
## Table 52. I/O current injection susceptibility

```
While a simple application is executed on the device, the device is stressed by injecting
current into the I/O pins programmed in floating input mode. While current is injected into
the I/O pin, one at a time, the device is checked for functional failures.
```
```
The failure is indicated by an out of range parameter: ADC error above a certain limit (>5
LSB TUE), out of conventional limits of induced leakage current on adjacent pins
(out of –5 μA/+0 μA range), or other functional failure (for example reset, oscillator
frequency deviation).
```
```
Negative induced leakage current is caused by negative injection and positive induced
leakage current by positive injection.
```
```
The test results are given in Table 52.
```
## Table 51. Electrical sensitivities

```
Symbol Parameter Conditions Class
```
```
LU Static latch-up class TA = + 125 °C conforming to JESD78A II level A
```
**Table 52. I/O current injection susceptibility(1)**

```
Symbol Description
```
```
Functional susceptibility
```
```
Negative Unit
injection
```
```
Positive
injection
```
##### IINJ

```
Injected current on BOOT0 pin –0 NA
```
```
mA
```
```
Injected current on NRST pin –0 NA
```
```
Injected current on PB3, PB4, PB5, PB6, PB7,
PB8, PB9, PC13, PC14, PC15, PH1, PDR_ON,
PC0, PC1,PC2, PC3, PD1, PD5, PD6, PD7, PE0,
PE2, PE3, PE4, PE5, PE6
```
##### –0 NA

```
Injected current on any other FT pin –5 NA
```
```
Injected current on any other pins –5 +5
```
1. NA = not applicable.


**Electrical characteristics STM32F411xC STM32F411xE**

98/151 DS10314 Rev 8

_Note: It is recommended to add a Schottky diode (pin to ground) to analog pins which may
potentially inject negative currents._

#### 6.3.16 I/O port characteristics

**General input/output characteristics**

```
Unless otherwise specified, the parameters given in Table 53 are derived from tests
performed under the conditions summarized in Table 14. All I/Os are CMOS and TTL
compliant.
```
## Table 53. I/O static characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
##### VIL

```
FT, TC and NRST I/O input low
level voltage 1.7 V≤^ VDD≤^ 3.6 V - - 0.3VDD
```
```
(1)
```
##### V

```
BOOT0 I/O input low level
voltage
```
##### 1.75 V≤ VDD ≤ 3.6 V,

##### -40 °C≤ TA ≤ 125 °C

##### --

##### 0.1VDD+0.1

```
(2)
1.7 V≤ VDD ≤ 3.6 V,
0 °C≤ TA ≤ 125 °C --
```
##### VIH

```
FT, TC and NRST I/O input high
level voltage(5) 1.7 V≤^ VDD≤^ 3.6 V
```
##### 0.7VDD(1

##### ) --

##### V

```
BOOT0 I/O input high level
voltage
```
##### 1.75 V≤ VDD ≤ 3.6 V,

##### -40 °C≤ TA ≤ 125 °C 0.17VDD

##### 1.7 V≤ V +0.7(2) --

##### DD ≤^ 3.6 V,

##### 0 °C≤ TA ≤ 125 °C

##### VHYS

```
FT, TC and NRST I/O input
hysteresis 1.7 V≤^ VDD≤^ 3.6 V - 10% VDD
```
##### (3) -V

```
BOOT0 I/O input hysteresis
```
##### 1.75 V≤ VDD ≤ 3.6 V,

##### -40 °C≤ TA ≤ 125 °C

```
-100 -mV
1.7 V≤ VDD ≤ 3.6 V,
0 °C≤ TA ≤ 125 °C
```
```
Ilkg
```
```
I/O input leakage current (4) VSS ≤ VIN ≤ VDD --± 1
```
```
I/O FT/TC input leakage current μA
(5) VIN = 5 V - -^3
```

```
DS10314 Rev 8 99/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
All I/Os are CMOS and TTL compliant (no software configuration required). Their
characteristics cover more than the strict CMOS-technology or TTL parameters. The
coverage of these requirements for FT and TC I/Os is shown in Figure 30.
```
##### RPU

```
Weak pull-up
equivalent
resistor(6)
```
```
All pins
except for
PA10
(OTG_FS_ID)
```
##### VIN = VSS 30 40 50

```
kΩ
```
##### PA10

##### (OTG_FS_ID) -71014

##### RPD

```
Weak pull-down
equivalent
resistor(7)
```
```
All pins
except for
PA10
(OTG_FS_ID)
```
##### VIN = VDD 30 40 50

##### PA10

##### (OTG_FS_ID) -71014

```
CIO(8) I/O pin capacitance - - 5 - pF
```
1. Guaranteed by test in production.
2. Guaranteed by design - Not tested in production.
3. With a minimum of 200 mV.
4. Leakage could be higher than the maximum value, if negative current is injected on adjacent pins, Refer to _Table 52: I/O_
    _current injection susceptibility_
5. To sustain a voltage higher than VDD +0.3 V, the internal pull-up/pull-down resistors must be disabled. Leakage could be
    higher than the maximum value, if negative current is injected on adjacent pins.Refer to _Table 52: I/O current injection_
    _susceptibility_
6. Pull-up resistors are designed with a true resistance in series with a switchable PMOS. This PMOS contribution to the
    series resistance is minimum (~10% order).
7. Pull-down resistors are designed with a true resistance in series with a switchable NMOS. This NMOS contribution to the
    series resistance is minimum (~10% order).
8. Hysteresis voltage between Schmitt trigger switching levels. Evaluated by characterization - Not tested in production.

```
Table 53. I/O static characteristics (continued)
```
```
Symbol Parameter Conditions Min Typ Max Unit
```

**Electrical characteristics STM32F411xC STM32F411xE**

100/151 DS10314 Rev 8

## Figure 30. FT/TC I/O input characteristics

**Output driving current**

```
The GPIOs (general purpose input/outputs) can sink or source up to ±8 mA, and sink or
source up to ±20 mA (with a relaxed VOL/VOH) except PC13, PC14 and PC15 which can
sink or source up to ±3mA. When using the PC13 to PC15 GPIOs in output mode, the speed
should not exceed 2 MHz with a maximum load of 30 pF.
```
```
In the user application, the number of I/O pins which can drive current must be limited to
respect the absolute maximum rating specified in Section 6.2. In particular:
```
- The sum of the currents sourced by all the I/Os on VDD, plus the maximum Run
    consumption of the MCU sourced on VDD, cannot exceed the absolute maximum rating
    ΣIVDD (see _Table 12_ ).
- The sum of the currents sunk by all the I/Os on VSS plus the maximum Run
    consumption of the MCU sunk on VSS cannot exceed the absolute maximum rating
    ΣIVSS (see _Table 12_ ).

```
MS33746V1
```
```
1.92
```
```
1.065
```
```
1.22
```
```
1.7 2.0 2.4 2.7 3.3 3.6
```
```
2.0
```
```
0.55
```
```
0.8
```
```
VDD (V)
```
```
VIL/VIH (V)
```
```
Tested in production - CMOS requirement VIHmin = 0.7VDD
```
```
Tested in production - CMOS requirement VILmax = 0.3VDD
```
```
Based on Design simulations, VILmax= 0.35VDD-0.04
```
```
TTL requirement
VIHmin = 2V
```
```
TTL requirement VILmax
0.51 = 0.8V
```
```
2.52
```
```
Area not
1.19 determined
```
```
1.7
```
```
Based on Design simulations, VIHmin= 0.45VDD+0.3
```

```
DS10314 Rev 8 101/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
**Output voltage levels**

```
Unless otherwise specified, the parameters given in Table 54 are derived from tests
performed under ambient temperature and VDD supply voltage conditions summarized in
Table 14. All I/Os are CMOS and TTL compliant.
```
## Table 54. Output voltage characteristics

```
Symbol Parameter Conditions Min Max Unit
```
```
VOL(1)
```
1. The IIO current sunk by the device must always respect the absolute maximum rating specified in _Table 12_.
    and the sum of IIO (I/O ports and control pins) must not exceed IVSS.

```
Output low level voltage for an I/O pin CMOS port(2)
IIO = +8 mA
2.7 V ≤ VDD ≤ 3.6 V
```
2. TTL and CMOS outputs are compatible with JEDEC standards JESD36 and JESD52.

##### -0.4

##### V

##### VOH(3)

3. The IIO current sourced by the device must always respect the absolute maximum rating specified in
    _Table 12_ and the sum of IIO (I/O ports and control pins) must not exceed IVDD.

```
Output high level voltage for an I/O pin VDD–0.4 -
```
```
VOL (1) Output low level voltage for an I/O pin TTL port(2)
IIO =+8 mA
2.7 V ≤ VDD ≤ 3.6 V
```
##### -0.4

##### V

```
VOH (3) Output high level voltage for an I/O pin 2.4 -
```
```
VOL(1) Output low level voltage for an I/O pin IIO = +20 mA
2.7 V ≤ VDD ≤ 3.6 V
```
##### -1.3(4)

4. Evaluated by characterization - Not tested in production.

##### V

```
VOH(3) Output high level voltage for an I/O pin VDD–1.3(4) -
```
```
VOL(1) Output low level voltage for an I/O pin IIO = +6 mA
1.8 V ≤ VDD ≤ 3.6 V
```
##### -0.4(4)

##### V

```
VOH(3) Output high level voltage for an I/O pin VDD–0.4(4) -
```
```
VOL(1) Output low level voltage for an I/O pin IIO = +4 mA
1.7 V ≤ VDD ≤ 3.6 V
```
##### -0.4(5)

5. Guaranteed by design - Not tested in production.

##### V

```
VOH(3) Output high level voltage for an I/O pin VDD–0.4(5) -
```

**Electrical characteristics STM32F411xC STM32F411xE**

102/151 DS10314 Rev 8

**Input/output AC characteristics**

```
The definition and values of input/output AC characteristics are given in Figure 31 and
Table 55 , respectively.
```
```
Unless otherwise specified, the parameters given in Table 55 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 14.
```
## Table 55. I/O AC characteristics

```
OSPEEDRy
[1:0] bit
value(1)
```
```
Symbol Parameter Conditions Min Typ Max Unit
```
##### 00

```
fmax(IO)out Maximum frequency(3)
```
```
CL = 50 pF, VDD ≥ 2.70 V - - 4
```
```
MHz
```
```
CL = 50 pF, VDD≥ 1.7 V - - 2
```
```
CL = 10 pF, VDD ≥ 2.70 V - - 8
```
```
CL = 10 pF, VDD ≥ 1.7 V - - 4
```
```
tf(IO)out/
tr(IO)out
```
```
Output high to low level fall
time and output low to high
level rise time
```
```
CL = 50 pF, VDD = 1.7 V to
3.6 V
```
```
--100ns
```
##### 01

```
fmax(IO)out Maximum frequency(3)
```
```
CL = 50 pF, VDD ≥ 2.70 V - - 25
```
```
MHz
```
```
CL = 50 pF, VDD ≥ 1.7 V - - 12.5
```
```
CL = 10 pF, VDD ≥ 2.70 V - - 50
```
```
CL = 10 pF, VDD ≥ 1.7 V - - 20
```
```
tf(IO)out/
tr(IO)out
```
```
Output high to low level fall
time and output low to high
level rise time
```
```
CL = 50 pF, VDD ≥2.7 V - - 10
```
```
ns
```
```
CL = 50 pF, VDD ≥ 1.7 V - - 20
```
```
CL = 10 pF, VDD ≥ 2.70 V - - 6
```
```
CL = 10 pF, VDD ≥ 1.7 V - - 10
```
##### 10

```
fmax(IO)out Maximum frequency(3)
```
```
CL = 40 pF, VDD ≥ 2.70 V - - 50 (4)
```
```
MHz
```
```
CL = 40 pF, VDD ≥ 1.7 V - - 25
```
```
CL = 10 pF, VDD ≥ 2.70 V - - 100 (4)
```
```
CL = 10 pF, VDD ≥ 1.7 V - - 50 (4)
```
```
tf(IO)out/
tr(IO)out
```
```
Output high to low level fall
time and output low to high
level rise time
```
```
CL = 40 pF, VDD≥ 2.70 V - - 6
```
```
ns
```
```
CL = 40 pF, VDD≥ 1.7 V - - 10
```
```
CL = 10 pF, VDD≥ 2.70 V - - 4
```
```
CL = 10 pF, VDD≥ 1.7 V - - 6
```

```
DS10314 Rev 8 103/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 31. I/O AC characteristics definition

##### 11

```
Fmax(IO)outMaximum frequency(3)
```
```
CL = 30 pF, VDD ≥ 2.70 V - - 100 (4)
```
```
CL = 30 pF, VDD ≥ 1.7 V - - 50 (4) MHz
```
```
tf(IO)out/
tr(IO)out
```
```
Output high to low level fall
time and output low to high
level rise time
```
```
CL = 30 pF, VDD ≥ 2.70 V - - 4
```
```
ns
```
```
CL = 30 pF, VDD ≥ 1.7 V - - 6
```
```
CL = 10 pF, VDD≥ 2.70 V - - 2.5
```
```
CL = 10 pF, VDD≥ 1.7 V - - 4
```
```
-tEXTIpw
```
```
Pulse width of external signals
detected by the EXTI
controller
```
```
10 - - ns
```
1. Evaluated by characterization - Not tested in production.
2. The I/O speed is configured using the OSPEEDRy[1:0] bits. Refer to the STM32F4xx reference manual for a description of
    the GPIOx_SPEEDR GPIO port output speed register.
3. The maximum frequency is defined in _Figure 31_.
4. For maximum frequencies above 50 MHz and VDD > 2.4 V, the compensation cell should be used.

```
Table 55. I/O AC characteristics(1)(2) (continued)
```
```
OSPEEDRy
[1:0] bit
value(1)
```
```
Symbol Parameter Conditions Min Typ Max Unit
```
```
ai14131d
```
```
10%
```
```
90%
```
```
50%
```
```
tr(IO)out
OUTPUT
```
```
EXTERNAL
```
```
ON CL
```
```
Maximum frequency is achieved if (tr + tf7DQGLIWKHGXW\F\FOHLV
ZKHQORDGHGE\&LVSHFLILHGLQWKHWDEOH³ I/O AC characteristics ”.
```
```
10%
```
```
50%
90%
```
```
T
```
```
tf(IO)out
```

**Electrical characteristics STM32F411xC STM32F411xE**

104/151 DS10314 Rev 8

#### 6.3.17 NRST pin characteristics

```
The NRST pin input driver uses CMOS technology. It is connected to a permanent pull-up
resistor, RPU (see Table 53 ).
```
```
Unless otherwise specified, the parameters given in Table 56 are derived from tests
performed under the ambient temperature and VDD supply voltage conditions summarized
in Table 14. Refer to Table 53: I/O static characteristics for the values of VIH and VIL for
NRST pin.
```
## Figure 32. Recommended NRST pin protection

1. The reset network protects the device against parasitic resets.
2. The user must ensure that the level on the NRST pin can go below the VIL(NRST) max level specified in
    _Table 56_. Otherwise the reset is not taken into account by the device.

## Table 56. NRST pin characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
##### RPU

```
Weak pull-up equivalent
resistor(1) VIN = VSS^304050 kΩ
```
```
VF(NRST)(2) NRST Input filtered pulse - - 100 ns
```
```
VNF(NRST)(2) NRST Input not filtered pulse VDD > 2.7 V 300 - - ns
```
```
TNRST_OUT Generated reset pulse duration
```
```
Internal Reset
source^20 - - μs
```
1. The pull-up is designed with a true resistance in series with a switchable PMOS. This PMOS contribution to the series
    resistance must be minimum (~10% order).
2. Guaranteed by design - Not tested in production.

```
ai14132c
```
```
STM32F
```
```
NRST(2) RPU
```
```
VDD
```
```
Filter
```
```
Internal Reset
```
```
0.1 μF
```
```
External
reset circuit(1)
```

```
DS10314 Rev 8 105/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.18 TIM timer characteristics

```
The parameters given in Table 57 are guaranteed by design.
```
```
Refer to Section 6.3.16: I/O port characteristics for details on the input/output alternate
function characteristics (output compare, input capture, external clock, PWM output).
```
#### 6.3.19 Communications interfaces

**I**

##### 2

**C interface**^ **characteristics**

```
The I^2 C interface meets the requirements of the standard I^2 C communication protocol with
the following restrictions: the I/O pins SDA and SCL are mapped to are not “true” open-
drain. When configured as open-drain, the PMOS connected between the I/O pin and VDD is
disabled, but is still present.
```
```
The I^2 C characteristics are described in Table 58. Refer also to^ Section 6.3.16: I/O port
characteristics for more details on the input/output alternate function characteristics (SDA
and SCL).
```
```
The I^2 C bus interface supports standard mode (up to 100 kHz) and fast mode (up to 400
kHz). The I^2 C bus frequency can be increased up to 1 MHz. For more details about the
complete solution, please contact your local ST sales representative.
```
## Table 57. TIMx characteristics

1. TIMx is used as a general term to refer to the TIM1 to TIM11 timers.
2. Guaranteed by design - Not tested in production.

```
Symbol Parameter Conditions(3)
```
3. The maximum timer frequency on APB1 is 50 MHz and on APB2 is up to 100 MHz, by setting the TIMPRE
    bit in the RCC_DCKCFGR register, if APBx prescaler is 1 or 2 or 4, then TIMxCLK = HCKL, otherwise
    TIMxCLK >= 4x PCLKx.

```
Min Max Unit
```
```
tres(TIM) Timer resolution time
```
```
AHB/APBx prescaler=1
or 2 or 4, fTIMxCLK =
100 MHz
```
```
1-tTIMxCLK
```
```
11.9 - ns
```
```
AHB/APBx prescaler>4,
fTIMxCLK = 100 MHz
```
```
1-tTIMxCLK
```
```
11.9 - ns
```
```
fEXT Timer external clock
frequency on CH1 to CH4 f
TIMxCLK = 100 MHz
```
```
0 fTIMxCLK/2 MHz
```
```
050MHz
```
```
ResTIM Timer resolution - 16/32 bit
```
```
tCOUNTER
```
```
16-bit counter clock
period when internal clock
is selected
```
```
fTIMxCLK = 100 MHz 0.0119 780 μs
```
```
tMAX_COUNT Maximum possible count
with 32-bit counter
```
##### - 65536 ×

##### 65536

```
tTIMxCLK
```
```
fTIMxCLK = 100 MHz -51.1S
```

**Electrical characteristics STM32F411xC STM32F411xE**

106/151 DS10314 Rev 8

## Table 58. I^2 C characteristics.

```
Symbol Parameter
```
```
Standard mode
I^2 C(1)(2)
```
1. Guaranteed by design - Not tested in production.

```
Fast mode I^2 C(1)(2)
```
2. fPCLK1 must be at least 2 MHz to achieve standard mode I^2 C frequencies. It must be at least 4 MHz to
    achieve fast mode I^2 C frequencies, and a multiple of 10 MHz to reach the 400 kHz maximum I^2 C fast mode
    clock.

```
Unit
Min Max Min Max
```
```
tw(SCLL) SCL clock low time 4.7 - 1.3 -
μs
tw(SCLH) SCL clock high time 4.0 - 0.6 -
```
```
tsu(SDA) SDA setup time 250 - 100 -
```
```
ns
```
```
th(SDA) SDA data hold time 0 3450 (3)
```
3. The device must internally provide a hold time of at least 300 ns for the SDA signal in order to bridge the
    undefined region of the falling edge of SCL.

##### 0900 (4)

4. The maximum data hold time has only to be met if the interface does not stretch the low period of SCL
    signal.

```
tr(SDA)
tr(SCL) SDA and SCL rise time -^1000 -^300
```
```
tf(SDA)
tf(SCL) SDA and SCL fall time -^300 -^300
```
```
th(STA) Start condition hold time 4.0 - 0.6 -
μs
tsu(STA)
```
```
Repeated Start condition
setup time 4.7 - 0.6 -
```
```
tsu(STO) Stop condition setup time 4.0 - 0.6 - μs
```
```
tw(STO:STA)
```
```
Stop to Start condition time
(bus free) 4.7 - 1.3 - μs
```
```
tSP
```
```
Pulse width of the spikes
that are suppressed by the
analog filter for standard fast
mode
```
##### 050 (5)

5. The minimum width of the spikes filtered by the analog filter is above tSP (max)

```
050 (5) ns
```
```
Cb
```
```
Capacitive load for each bus
line -^400 -^400 pF
```

```
DS10314 Rev 8 107/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 33. I^2 C bus AC waveforms and measurement circuit

1. RS = series protection resistor.
2. RP = external pull-up resistor.
3. VDD_I2C is the I2C bus power supply.

## Table 59. SCL frequency (fPCLK1= 50 MHz, VDD = VDD_I2C = 3.3 V)

1. RP = External pull-up resistance, fSCL = I^2 C speed
2. For speeds around 200 kHz, the tolerance on the achieved speed is of ±5%. For other speed ranges, the
    tolerance on the achieved speed is ±2%. These variations depend on the accuracy of the external
    components used to design the application.

```
fSCL (kHz)
```
```
I2C_CCR value
```
```
RP = 4.7 k Ω
```
```
400 0x8019
```
```
300 0x8021
```
```
200 0x8032
```
```
100 0x0096
```
```
50 0x012C
```
```
20 0x02EE
```
```
ai14979d
```
```
START
```
```
SD A
```
```
RP
```
```
I²C bus
```
```
VDD_I2C
```
```
STM32
SDA
SCL
```
```
tf(SDA) tr(SDA)
```
```
SCL
```
```
th(STA)
```
```
tw(SCLH)
```
```
tw(SCLL)
```
```
tsu(SDA)
```
```
tr(SCL) tf(SCL)
```
```
th(SDA)
```
```
START REPEATED
```
```
tsu(STA)
```
```
tsu(STO)
```
```
STOP tw(STO:STA)
```
```
VDD_I2C
```
```
RP R
S
```
```
RS
```
```
START
```

**Electrical characteristics STM32F411xC STM32F411xE**

108/151 DS10314 Rev 8

**SPI interface characteristics**

```
Unless otherwise specified, the parameters given in Table 60 for the SPI interface are
derived from tests performed under the ambient temperature, fPCLKx frequency and VDD
supply voltage conditions summarized in Table 14 , with the following configuration:
```
- Output speed is set to OSPEEDRy[1:0] = 10
- Capacitive load C = 30 pF
- Measurement points are done at CMOS levels: 0.5VDD

```
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (NSS, SCK, MOSI, MISO for SPI).
```
## Table 60. SPI dynamic characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fSCK
1/tc(SCK)
```
```
SPI clock frequency
```
```
Master full duplex/receiver mode,
2.7 V < VDD < 3.6 V
SPI1/4/5
```
##### --42

```
MHz
```
```
Master full duplex/receiver mode,
3.0 V < VDD < 3.6 V
SPI1/4/5
```
--^50

```
Master transmitter mode
1.7 V < VDD < 3.6 V
SPI1/4/5
```
--^50

```
Master mode
1.7 V < VDD < 3.6 V
SPI1/2/3/4/5
```
--^25

```
Slave transmitter/full duplex mode
2.7 V < VDD < 3.6 V
SPI1/4/5
```
--^38

```
(2)
```
```
Slave receiver mode,
1.8 V < VDD < 3.6 V
SPI1/4/5
```
##### --50

```
Slave mode,
1.8 V < VDD < 3.6 V
SPI1/2/3/4/5
```
--^25

```
Duty(SCK)
```
```
Duty cycle of SPI clock
frequency Slave mode^305070 %
```
```
tw(SCKH)
tw(SCKL)
```
```
SCK high and low time Master mode, SPI presc = 2 TPCLK−1.5 TPCLK
```
##### TPCLK

```
+1.5 ns
```
```
tsu(NSS) NSS setup time Slave mode, SPI presc = 2 3TPCLK --ns
```
```
th(NSS) NSS hold time Slave mode, SPI presc = 2 2TPCLK --ns
```
```
tsu(MI)
Data input setup time
```
```
Master mode 4 - - ns
```
```
tsu(SI) Slave mode 2.5 - - ns
```
```
th(MI)
Data input hold time
```
```
Master mode 7.5 - - ns
```
```
th(SI) Slave mode 3.5 - - ns
```

```
DS10314 Rev 8 109/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Figure 34. SPI timing diagram - slave mode and CPHA =

```
ta(SO) Data output access time Slave mode 7 - 21 ns
```
```
tdis(SO) Data output disable time Slave mode 5 - 12 ns
```
```
tv(SO) Data output valid time
```
```
Slave mode (after enable edge),
2.7 V < VDD < 3.6 V -1113ns
```
```
Slave mode (after enable edge),
1.7 V < VDD < 3.6 V -1118.5ns
```
```
th(SO) Data output hold time
```
```
Slave mode (after enable edge),
1.7 V < VDD < 3.6 V 8--ns
```
```
tv(MO) Data output valid time Master mode (after enable edge) - 4 6 ns
```
```
th(MO) Data output hold time Master mode (after enable edge) 0 - - ns
```
1. Evaluated by characterization - Not tested in production.
2. Maximum frequency in Slave transmitter mode is determined by the sum of tv(SO) and tsu(MI) which has to fit into SCK low or
    high phase preceding the SCK sampling edge. This value can be achieved when the SPI communicates with a master
    having tsu(MI) = 0 while Duty(SCK) = 50%

```
Table 60. SPI dynamic characteristics(1) (continued)
```
```
Symbol Parameter Conditions Min Typ Max Unit
```
```
SCK input
```
```
(SI)
```
```
MSB IN BIT1 IN LSB IN
```
```
MSB OUT BIT6 OUT LSB OUT
```
```
NSS input
```
```
MOSI
INPUT
```
```
MISO
OUTPUT
```
```
(SI)
```

**Electrical characteristics STM32F411xC STM32F411xE**

110/151 DS10314 Rev 8

## Figure 35. SPI timing diagram - slave mode and CPHA = 1(1)

## Figure 36. SPI timing diagram - master mode(1)

```
ai14135b
```
```
NSS input
```
```
tSU(NSS) tc(SCK) th(NSS)
```
```
SCK input
```
```
CPHA=1
CPOL=0
CPHA=1
CPOL=1
```
```
tw(SCKH)
tw(SCKL)
```
```
ta(SO) tv(SO) th(SO)
```
```
tr(SCK)
tf(SCK)
```
```
tdis(SO)
```
```
MISO
OUTPUT
```
```
MOSI
INPUT
```
```
tsu(SI) th(SI)
```
```
MSB OUT
```
```
MSB IN
```
```
BIT6 OUT LSB OUT
```
```
BIT 1 IN LSB IN
```
```
SCK Output
```
```
CPHA=0
```
```
MOSI
OUTPUT
```
```
MISO
INPUT
```
```
CPHA=0
```
```
LSB OUT
```
```
LSB IN
```
```
CPOL=0
```
```
CPOL=1
```
```
BIT1 OUT
```
```
NSS input
tc(SCK)
```
```
tw(SCKH)
tw(SCKL)
```
```
tr(SCK)
tf(SCK)
```
```
th(MI)
```
```
High
```
```
SCK Output
```
```
CPHA=1
```
```
CPHA=1
```
```
CPOL=0
```
```
CPOL=1
```
```
tsu(MI)
```
```
tv(MO) th(MO)
```
```
MSB IN BIT6 IN
```
```
MSB OUT
```
```
ai14136c
```

```
DS10314 Rev 8 111/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Table 61. I^2 S dynamic characteristics

```
Unless otherwise specified, the parameters given in Table 61 for the I^2 S interface are
derived from tests performed under the ambient temperature, fPCLKx frequency and VDD
supply voltage conditions summarized in Table 14 , with the following configuration:
```
- Output speed is set to OSPEEDRy[1:0] = 10
- Capacitive load C = 30 pF
- Measurement points are done at CMOS levels: 0.5VDD

```
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output alternate
function characteristics (CK, SD, WS).
```
_Note: Refer to the I2S section of RM0383 reference manual for more details on the sampling
frequency (FS).
fMCK, fCK, and DCK values reflect only the digital peripheral behavior. The values of these
parameters might be slightly impacted by the source clock precision. DCK depends mainly
on the value of ODD bit. The digital contribution leads to a minimum value of
(I2SDIV/(2*I2SDIV+ODD) and a maximum value of (I2SDIV+ODD)/(2*I2SDIV+ODD). FS
maximum value is supported for each mode/condition._

```
Table 61. I^2 S dynamic characteristics(1)
```
```
Symbol Parameter Conditions Min Max Unit
```
```
fMCK I2S Main clock output - 256x8K 256xFs(2) MHz
```
```
fCK I2S clock frequency
```
```
Master data: 32 bits - 64xFs
MHz
Slave data: 32 bits - 64xFs
```
```
DCK I2S clock frequency duty cycle Slave receiver 30 70 %
```
```
tv(WS) WS valid time Master mode 0 7
```
```
ns
```
```
th(WS) WS hold time Master mode 1.5 -
```
```
tsu(WS) WS setup time Slave mode 1.5 -
```
```
th(WS) WS hold time Slave mode 3 -
```
```
tsu(SD_MR)
Data input setup time
```
```
Master receiver 1 -
```
```
tsu(SD_SR) Slave receiver 2.5 -
```
```
th(SD_MR)
Data input hold time
```
```
Master receiver 7 -
```
```
th(SD_SR) Slave receiver 2.5 -
```
```
tv(SD_ST)
Data output valid time
```
```
Slave transmitter (after enable edge) - 20
```
```
tv(SD_MT) Master transmitter (after enable edge) - 6
```
```
th(SD_ST)
Data output hold time
```
```
Slave transmitter (after enable edge) 8 -
```
```
th(SD_MT) Master transmitter (after enable edge) 2 -
```
1. Evaluated by characterization - Not tested in production.
2. The maximum value of 256xFs is 50 MHz (APB1 maximum frequency).


**Electrical characteristics STM32F411xC STM32F411xE**

112/151 DS10314 Rev 8

## Figure 37. I^2 S slave timing diagram (Philips protocol)(1)

1. LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
    byte.

## Figure 38. I^2 S master timing diagram (Philips protocol)(1).

1. LSB transmit/receive of the previously transmitted byte. No LSB transmit/receive is sent before the first
    byte.


```
DS10314 Rev 8 113/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
**USB OTG full speed (FS) characteristics**

```
This interface is present in USB OTG FS controller.
```
_Note: When VBUS sensing feature is enabled, PA9 should be left at their default state (floating
input), not as alternate function. A typical 200 μA current consumption of the embedded
sensing block (current to voltage conversion to determine the different sessions) can be
observed on PA9 when the feature is enabled._

## Table 62. USB OTG FS startup time

```
Symbol Parameter Max Unit
```
```
tSTARTUP(1)
```
1. Guaranteed by design - Not tested in production.

```
USB OTG FS transceiver startup time 1 μs
```
## Table 63. USB OTG FS DC electrical characteristics.

```
Symbol Parameter Conditions Min.(1)
```
1. All the voltages are measured from the local ground potential.

```
Typ. Max.(1) Unit
```
```
Input
levels
```
##### VDD

```
USB OTG FS operating
voltage 3.0
```
```
(2)
```
2. The USB OTG FS functionality is ensured down to 2.7 V but not the full USB full speed electrical
    characteristics which are degraded in the 2.7-to-3.0 V VDD voltage range.

##### -3.6V

##### VDI(3)

3. Guaranteed by design - Not tested in production.

```
Differential input sensitivity I(USB_FS_DP/DM) 0.2 - -
```
##### VCM V

```
(3) Differential common mode
range Includes VDI range 0.8 - 2.5
```
##### VSE(3)

```
Single ended receiver
threshold 1.3 - 2.0
```
```
Output
levels
```
```
VOL Static output level low RL of 1.5 kΩ to 3.6 V(4)
```
4. RL is the load connected on the USB OTG FS drivers.

##### --0.3

##### V

```
VOH Static output level high RL of 15 kΩ to VSS(4) 2.8 - 3.6
```
##### RPD

##### PA11, PA12

##### (USB_FS_DM/DP) VIN = VDD^172124

```
kΩ
```
##### PA9 (OTG_FS_VBUS) 0.65 1.1 2.0

##### RPU

##### PA11, PA12

##### (USB_FS_DM/DP) VIN = VSS 1.5 1.8 2.1

##### PA9 (OTG_FS_VBUS) VIN = VSS 0.25 0.37 0.55


**Electrical characteristics STM32F411xC STM32F411xE**

114/151 DS10314 Rev 8

## Figure 39. USB OTG FS timings: definition of data signal rise and fall time

#### 6.3.20 12-bit ADC characteristics

```
Unless otherwise specified, the parameters given in Table 65 are derived from tests
performed under the ambient temperature, fPCLK2 frequency and VDDA supply voltage
conditions summarized in Table 14.
```
## Table 64. USB OTG FS electrical characteristics

1. Guaranteed by design - Not tested in production.

```
Driver characteristics
```
```
Symbol Parameter Conditions Min Max Unit
```
```
tr Rise time(2)
```
2. Measured from 10% to 90% of the data signal. For more detailed informations, please refer to USB
    Specification - Chapter 7 (version 2.0).

```
CL = 50 pF^ 420ns
```
```
tf Fall time(2) CL = 50 pF 4 20 ns
trfm Rise/ fall time matching tr/tf 90 110 %
```
```
VCRS Output signal crossover voltage 1.3 2.0 V
```
```
ai14137b
```
```
Cross over
points
Differential
data lines
```
```
VCRS
```
```
VSS
tf tr
```
## Table 65. ADC characteristics

```
Symbol Parameter Conditions Min Typ^ Max Unit
```
```
VDDA Power supply
VDDA − VREF+ < 1.2 V
```
##### 1.7(1) -3.6V

```
VREF+ Positive reference voltage 1.7(1) -VDDA V
```
```
fADC ADC clock frequency
```
```
VDDA = 1.7(1) to 2.4 V 0.6 15 18 MHz
VDDA = 2.4 to 3.6 V 0.6 30 36 MHz
```
```
fTRIG(2) External trigger frequency
```
```
fADC = 30 MHz,
12-bit resolution
```
- - 1764 kHz

```
--171/fADC
```
```
VAIN Conversion voltage range(3) 0 (VSSA or VREF-^
tied to ground)
```
##### -VREF+ V

```
RAIN(2) External input impedance
```
```
See Equation 1 for
details --50kΩ
```
```
RADC(2)(4) Sampling switch resistance - - 6 kΩ
```
```
CADC(2)
```
```
Internal sample and hold
capacitor -47pF
```

```
DS10314 Rev 8 115/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
```
tlat(2)
```
```
Injection trigger conversion
latency
```
```
fADC = 30 MHz - - 0.100 μs
```
```
--3(5) 1/fADC
```
```
tlatr(2)
```
```
Regular trigger conversion
latency
```
```
fADC = 30 MHz - - 0.067 μs
```
```
--2(5) 1/fADC
```
```
tS(2) Sampling time
```
```
fADC = 30 MHz 0.100 - 16 μs
```
```
3 - 480 1/fADC
tSTAB(2) Power-up time - 2 3 μs
```
```
tCONV(2) Total conversion time (including
sampling time)
```
```
fADC = 30 MHz
12-bit resolution
```
```
0.50 - 16.40 μs
```
```
fADC = 30 MHz
10-bit resolution
```
```
0.43 - 16.34 μs
```
```
fADC = 30 MHz
8-bit resolution
```
```
0.37 - 16.27 μs
```
```
fADC = 30 MHz
6-bit resolution
```
```
0.30 - 16.20 μs
```
```
9 to 492 (tS for sampling +n-bit resolution for successive
approximation)
```
```
1/fADC
```
```
fS(2)
```
```
Sampling rate
(fADC = 30 MHz, and
tS = 3 ADC cycles)
```
```
12-bit resolution
Single ADC
```
- - 2 Msps

```
12-bit resolution
Interleave Dual ADC
mode
```
- - 3.75 Msps

```
12-bit resolution
Interleave Triple ADC
mode
```
- - 6 Msps

##### IVREF+(2)

```
ADC VREF DC current
consumption in conversion
mode
```
- 300 500 μA

##### IVDDA(2)

```
ADC VDDA DC current
consumption in conversion
mode
```
```
-1.61.8mA
```
1. VDDA minimum value of 1.7 V is possible with the use of an external power supply supervisor (refer to _Section 3.15.2:_
    _Internal reset OFF_ ).
2. Evaluated by characterization - Not tested in production.
3. VREF+ is internally connected to VDDA and VREF- is internally connected to VSSA.
4. RADC maximum value is given for VDD=1.7 V, and minimum value for VDD=3.3 V.
5. For external triggers, a delay of 1/fPCLK2 must be added to the latency specified in _Table 65_.

```
Table 65. ADC characteristics (continued)
```
```
Symbol Parameter Conditions Min Typ^ Max Unit
```

**Electrical characteristics STM32F411xC STM32F411xE**

116/151 DS10314 Rev 8

```
Equation 1: RAIN max formula
```
```
The formula above ( Equation 1 ) is used to determine the maximum external impedance
allowed for an error below 1/4 of LSB. N = 12 (from 12-bit resolution) and k is the number of
sampling periods defined in the ADC_SMPR1 register.
```
## Table 66. ADC accuracy at fADC = 18 MHz

1. Better performance could be achieved in restricted VDD, frequency and temperature ranges.

```
Symbol Parameter Test conditions Typ Max(2)
```
2. Evaluated by characterization - Not tested in production.

```
Unit
```
```
ET Total unadjusted error
fADC =18 MHz
VDDA = 1.7 to 3.6 V
VREF = 1.7 to 3.6 V
VDDA − VREF < 1.2 V
```
##### ±3 ±4

##### LSB

```
EO Offset error ±2 ±3
```
```
EG Gain error ±1 ±3
```
```
ED Differential linearity error ±1 ±2
```
```
EL Integral linearity error ±2 ±3
```
## Table 67. ADC accuracy at fADC = 30 MHz

1. Better performance could be achieved in restricted VDD, frequency and temperature ranges.

```
Symbol Parameter Test conditions Typ Max(2)
```
2. Evaluated by characterization - Not tested in production.

```
Unit
```
```
ET Total unadjusted error
fADC = 30 MHz,
RAIN < 10 kΩ,
VDDA = 2.4 to 3.6 V,
VREF = 1.7 to 3.6 V,
VDDA − VREF < 1.2 V
```
##### ±2 ±5

##### LSB

```
EO Offset error ±1.5 ±2.5
```
```
EG Gain error ±1.5 ±4
```
```
ED Differential linearity error ±1 ±2
```
```
EL Integral linearity error ±1.5 ±3
```
## Table 68. ADC accuracy at fADC = 36 MHz

1. Better performance could be achieved in restricted VDD, frequency and temperature ranges.

```
Symbol Parameter Test conditions Typ Max(2)
```
2. Evaluated by characterization - Not tested in production.

```
Unit
```
```
ET Total unadjusted error
fADC =36 MHz,
VDDA = 2.4 to 3.6 V,
VREF = 1.7 to 3.6 V
VDDA − VREF < 1.2 V
```
##### ±4 ±7

##### LSB

```
EO Offset error ±2 ±3
```
```
EG Gain error ±3 ±6
```
```
ED Differential linearity error ±2 ±3
```
```
EL Integral linearity error ±3 ±6
```
```
RAIN
```
#### ()k0.5–

```
fADC CADC 2
```
##### N+ 2

```
× ×ln()
```
#### = ----------------------------------------------------------------–RADC


```
DS10314 Rev 8 117/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
Note: ADC accuracy vs. negative injection current: injecting a negative current on any analog
input pins should be avoided as this significantly reduces the accuracy of the conversion
being performed on another analog input. It is recommended to add a Schottky diode (pin to
ground) to analog pins which may potentially inject negative currents.
Any positive injection current within the limits specified for IINJ(PIN) and ΣIINJ(PIN) in
_Section 6.3.16_ does not affect the ADC accuracy.

## Table 69. ADC dynamic accuracy at fADC = 18 MHz - limited test conditions

```
Symbol Parameter Test conditions Min Typ Max Unit
```
```
ENOB Effective number of bits
fADC =18 MHz
VDDA = VREF+= 1.7 V
Input Frequency = 20 KHz
Temperature = 25 °C
```
```
10.3 10.4 - bits
```
```
SINAD Signal-to-noise and distortion ratio 64 64.2 -
```
```
SNR Signal-to-noise ratio 64 65 - dB
```
```
THD Total harmonic distortion - -72 -67
```
1. Evaluated by characterization - Not tested in production.

## Table 70. ADC dynamic accuracy at fADC = 36 MHz - limited test conditions

```
Symbol Parameter Test conditions Min Typ Max Unit
```
```
ENOB Effective number of bits
fADC = 36 MHz
VDDA = VREF+ = 3.3 V
Input Frequency = 20 KHz
Temperature = 25 °C
```
```
10.6 10.8 - bits
```
```
SINAD Signal-to noise and distortion ratio 66 67 -
```
```
SNR Signal-to noise ratio 64 68 - dB
```
```
THD Total harmonic distortion - -72 -70
```
1. Evaluated by characterization - Not tested in production.


**Electrical characteristics STM32F411xC STM32F411xE**

118/151 DS10314 Rev 8

## Figure 40. ADC accuracy characteristics

1. See also _Table 67_.
2. Example of an actual transfer curve.
3. Ideal transfer curve.
4. End point correlation line.
5. ET = Total Unadjusted Error: maximum deviation between the actual and the ideal transfer curves.
    EO = Offset Error: deviation between the first actual transition and the first ideal one.
    EG = Gain Error: deviation between the last ideal transition and the last actual one.
    ED = Differential Linearity Error: maximum deviation between actual steps and the ideal one.
    EL = Integral Linearity Error: maximum deviation between any actual transition and the end point
    correlation line.

## Figure 41. Typical connection diagram using the ADC

1. Refer to _Table 65_ for the values of RAIN, RADC and CADC.
2. Cparasitic represents the capacitance of the PCB (dependent on soldering and PCB layout quality) plus the
    pad capacitance (roughly 5 pF). A high Cparasitic value downgrades conversion accuracy. To remedy this,
    fADC should be reduced.

```
ai14395c
```
```
EO
```
```
EG
```
```
1L SBIDEAL
```
```
4095
4094
4093
```
```
5 4 3 2 1 0
```
```
7
6
```
```
1 2 3 456 7 4093 4094 4095 4096
```
```
(1)
```
```
(2)
ET
```
```
ED
```
```
EL
```
```
(3)
```
```
VSSA VDDA
```
```
VREF+
4096
```
```
(or depending on package)]
```
```
VDDA
4096
```
```
[1LSBIDEAL =
```
```
ai17534
```
```
VDD STM32F
```
```
AINx
```
```
IL±1 μA
```
```
0.6 V
```
```
VT
```
```
RAIN(1)
```
```
Cparasitic
```
```
VAIN
```
```
0.6 V
```
```
VT
RADC(1)
```
```
CADC(1)
```
```
12-bit
converter
```
```
Sample and hold ADC
converter
```

```
DS10314 Rev 8 119/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
**General PCB design guidelines**

```
Power supply decoupling should be performed as shown in Figure 42 or Figure 43 ,
depending on whether VREF+ is connected to VDDA or not. The 10 nF capacitors should be
ceramic (good quality). They should be placed them as close as possible to the chip.
```
## Figure 42. Power supply and reference decoupling (VREF+ not connected to VDDA).

1. VREF+ and VREF- inputs are both available on UFBGA100. VREF+ is also available on LQFP100. When
    VREF+ and VREF- are not available, they are internally connected to VDDA and VSSA.

```
STM32F
```
```
1 μF // 10 nF
```
```
1 μF // 10 nF
```
```
VREF+^ (1)
```
```
VDDA
```
```
VSSA/VREF-^ (1)
```
```
ai17535b
```

**Electrical characteristics STM32F411xC STM32F411xE**

120/151 DS10314 Rev 8

## Figure 43. Power supply and reference decoupling (VREF+ connected to VDDA).

1. VREF+ and VREF- inputs are both available on UFBGA100. VREF+ is also available on LQFP100. When
    VREF+ and VREF- are not available, they are internally connected to VDDA and VSSA.

#### 6.3.21 Temperature sensor characteristics

```
STM32F
```
```
1 μF // 10 nF
```
```
ai17536c
```
```
VREF+/VDDA
```
```
VREF-/VSSA(1)
```
```
(1)
```
## Table 71. Temperature sensor characteristics

```
Symbol Parameter Min Typ Max Unit
```
```
TL(1) VSENSE linearity with temperature - ± 1 ±2°C
```
```
Avg_Slope(1) Average slope - 2.5 - mV/°C
```
```
V 25 (1) Voltage at 25 °C - 0.76 - V
```
```
tSTART(2) Startup time - 6 10 μs
```
```
TS_temp(2) ADC sampling time when reading the temperature (1 °C accuracy) 10 - - μs
```
1. Evaluated by characterization - Not tested in production.
2. Guaranteed by design - Not tested in production.

## Table 72. Temperature sensor calibration values.

```
Symbol Parameter Memory address
```
```
TS_CAL1 TS ADC raw data acquired at temperature of 30 °C, VDDA= 3.3 V 0x1FFF 7A2C - 0x1FFF 7A2D
```
```
TS_CAL2 TS ADC raw data acquired at temperature of 110 °C, VDDA= 3.3 V 0x1FFF 7A2E - 0x1FFF 7A2F
```

```
DS10314 Rev 8 121/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
#### 6.3.22 VBAT monitoring characteristics

#### 6.3.23 Embedded reference voltage

```
The parameters given in Table 74 are derived from tests performed under ambient
temperature and VDD supply voltage conditions summarized in Table 14.
```
## Table 73. VBAT monitoring characteristics

```
Symbol Parameter Min Typ Max Unit
```
```
R Resistor bridge for VBAT -50-KΩ
```
```
Q Ratio on VBAT measurement - 4 -
```
```
Er(1) Error on Q –1 - +1 %
```
```
TS_vbat(2)(2) ADC sampling time when reading the VBAT^
1 mV accuracy
```
```
5- -μs
```
1. Guaranteed by design - Not tested in production.
2. Shortest sampling time can be determined in the application by multiple iterations.

## Table 74. Embedded internal reference voltage

```
Symbol Parameter Conditions Min Typ^ Max Unit
```
```
VREFINT Internal reference voltage - 40 °C < TA < + 125 °C 1.18 1.21 1.24 V
```
```
TS_vrefint(1)
```
```
ADC sampling time when reading the
internal reference voltage -10--μs
```
```
VRERINT_s(2)
```
```
Internal reference voltage spread over the
temperature range VDD = 3V ± 10mV -^35 mV
```
```
TCoeff(2) Temperature coefficient - - 30 50 ppm/°C
```
```
tSTART(2) Startup time - - 6 10 μs
```
1. Shortest sampling time can be determined in the application by multiple iterations.
2. Guaranteed by design - Not tested in production.

## Table 75. Internal reference voltage calibration values

```
Symbol Parameter Memory address
```
```
VREFIN_CAL Raw data acquired at temperature of
30 °C VDDA = 3.3 V
```
```
0x1FFF 7A2A - 0x1FFF 7A2B
```

**Electrical characteristics STM32F411xC STM32F411xE**

122/151 DS10314 Rev 8

#### 6.3.24 SD/SDIO MMC/eMMC card host interface (SDIO) characteristics

```
Unless otherwise specified, the parameters given in Table 76 for the SDIO/MMC/eMMC
interface are derived from tests performed under the ambient temperature, fPCLK2 frequency
and VDD supply voltage conditions summarized in Table 14 , with the following configuration:
```
- Output speed is set to OSPEEDRy[1:0] = 10
- Capacitive load C = 30 pF (for eMMC C = 20 pF)
- Measurement points are done at CMOS levels: 0.5VDD

```
Refer to Section 6.3.16: I/O port characteristics for more details on the input/output
characteristics.
```
## Figure 44. SDIO high-speed mode

## Figure 45. SD default mode

```
ai14888
```
```
CK
```
```
D, CMD
(output)
```
```
tOVD tOHD
```

```
DS10314 Rev 8 123/151
```
**STM32F411xC STM32F411xE Electrical characteristics**

```
124
```
## Table 76. Dynamic characteristics: SD / MMC characteristics

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fPP
```
```
Clock frequency in data transfer
mode -0-50MHz
```
- SDIO_CK/fPCLK2 frequency ratio - - - 8/3 -

```
tW(CKL) Clock low time fpp = 50 MHz 10.5 11 -
ns
tW(CKH) Clock high time fpp = 50 MHz 8.5 9 -
```
```
CMD, D inputs (referenced to CK) in MMC and SD HS mode
```
```
tISU Input setup time HS fpp = 50 MHz 2.5 - -
```
```
ns
tIH Input hold time HS
```
```
fpp = 50 MHz
-40°C<TA< 125°C
```
##### 5- -

```
fpp = 50 MHz
-40°C<TA<+85°C
```
##### 2.5 - -

```
CMD, D outputs (referenced to CK) in MMC and SD HS mode
```
```
tOV Output valid time HS fpp = 50 MHz - 3.5 4
ns
tOH Output hold time HS fpp = 50 MHz 2 - -
```
```
CMD, D inputs (referenced to CK) in SD default mode
```
```
tISUD Input setup time SD fpp = 25 MHz 3 - -
ns
tIHD Input hold time SD fpp = 25 MHz 4 - -
```
```
CMD, D outputs (referenced to CK) in SD default mode
```
```
tOVD Output valid default time SD fpp =25 MHz - 5 5.5
ns
tOHD Output hold default time SD fpp =25 MHz 4.5 - -
```
1. Evaluated by characterization - Not tested in production.
2. VDD = 2.7 to 3.6 V.


**Electrical characteristics STM32F411xC STM32F411xE**

124/151 DS10314 Rev 8

#### 6.3.25 RTC characteristics

## Table 77. Dynamic characteristics: eMMC characteristics VDD = 1.7 V to 1.9 V

1. Evaluated by characterization - Not tested in production.
2. Cload = 20 pF

```
Symbol Parameter Conditions Min Typ Max Unit
```
```
fPP
```
```
Clock frequency in data transfer
mode -0-50MHz
```
- SDIO_CK/fPCLK2 frequency ratio - - - 8/3 -

```
tW(CKL) Clock low time fpp = 50 MHz 10 10.5 -
ns
tW(CKH) Clock high time fpp = 50 MHz 9 9.5 -
```
```
CMD, D inputs (referenced to CK) in eMMC mode
```
```
tISU Input setup time HS fpp = 50 MHz 0 - - ns
```
```
tIH Input hold time HS fpp = 50 MHz 6 - -
```
```
CMD, D outputs (referenced to CK) in eMMC mode
```
```
tOV Output valid time HS fpp = 50 MHz - 3.5 5
ns
tOH Output hold time HS fpp = 50 MHz 2 - -
```
## Table 78. RTC characteristics

```
Symbol Parameter Conditions Min Max
```
```
-fPCLK1/RTCCLK frequency ratio
```
```
Any read/write operation
from/to an RTC register 4-
```

```
DS10314 Rev 8 125/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
## 7 Package information

```
In order to meet environmental requirements, ST offers these devices in different grades of
ECOPACK packages, depending on their level of environmental compliance. ECOPACK
specifications, grade definitions and product status are available at: http://www.st.com.
ECOPACK is an ST trademark.
```
### 7.1 Device marking

```
Refer to technical note “Reference device marking schematics for STM32 microcontrollers
and microprocessors” (TN1433) available on http://www.st.com , for the location of pin 1 / ball A1
as well as the location and orientation of the marking areas versus pin 1 / ball A1.
```
```
Parts marked as “ES”, “E” or accompanied by an engineering sample notification letter, are
not yet qualified and therefore not approved for use in production. ST is not responsible for
any consequences resulting from such use. In no event will ST be liable for the customer
using any of these engineering samples in production. ST’s Quality department must be
contacted prior to any decision to use these engineering samples to run a qualification
activity.
```

**Package information STM32F411xC STM32F411xE**

126/151 DS10314 Rev 8

### 7.2 WLCSP49 package information (A0ZV)

```
This WLCSP is a 49-ball, 2.999 x 3.185 mm, 0.4 mm pitch wafer level chip scale
```
## Figure 46. WLCSP49 - Outline

1. Drawing is not to scale.

```
A1 orientation
reference
```
```
Wafer back side
```
```
E
```
```
D
```
```
Detail A
(rotated 90 °)
```
```
Seating plane
Note 1
```
```
A1
```
```
Bump
```
```
b
```
```
Side view
```
```
A
A2
```
```
Detail A
```
(^71)
G
A
e1
F
G
e
e
A1 ball location
e2E
A3
Bump side
eeeZ
Note 2
Front view
A0ZV_ME_V1
ccc
ddd
bbb
Z
Z
ZXY
Z
aaa
(4X)


```
DS10314 Rev 8 127/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
## Figure 47. WLCSP49 - Footprint example

## Table 79. WLCSP49 - Mechanical data

```
Symbol
```
```
millimeters inches(1)
```
1. Values in inches are converted from mm and rounded to 4 decimal digits.

```
Min Typ Max Min Typ Max
```
```
A 0.525 0.555 0.585 0.0207 0.0219 0.0230
```
```
A1 - 0.175 - - 0.0069 -
```
```
A2 - 0.380 - - 0.0150 -
```
```
A3(2)
```
2. Back side coating

##### - 0.025 - - 0.0010 -

```
b(3)
```
3. Dimension is measured at the maximum bump diameter parallel to primary datum Z.

##### 0.220 0.250 0.280 0.0087 0.0098 0.0110

##### D 2.964 2.999 3.034 0. 1167 0.1181 0.1194

##### E 3.150 3.185 3.220 0.1240 0.1254 0.1268

```
e - 0.400 - - 0.0157 -
```
```
e1 - 2.400 - - 0.0945 -
```
```
e2 - 2.400 - - 0.0945 -
```
```
F - 0.2995 - - 0.0118 -
```
```
G - 0.3925 - - 0.0155 -
```
```
aaa - - 0.100 - - 0.0039
```
```
bbb - - 0.100 - - 0.0039
```
```
ccc - - 0.100 - - 0.0039
```
```
ddd - - 0.050 - - 0.0020
```
```
eee - - 0.050 - - 0.0020
```
```
Dsm MS18965V2
```
```
Dpad
```

**Package information STM32F411xC STM32F411xE**

128/151 DS10314 Rev 8

**Device marking for WLCSP49**

```
The following figure gives an example of topside marking orientation versus ball A1 identifier
location. The printed markings may differ depending on the supply chain.
```
```
Other optional marking or inset/upset marks, which depend on supply chain operations, are
not indicated below.
```
## Figure 48. WLCSP49 marking (package top view)

1. Parts marked as ES or E or accompanied by an Engineering Sample notification letter are not yet qualified
    and therefore not approved for use in production. ST is not responsible for any consequences resulting
    from such use. In no event will ST be liable for the customer using any of these engineering samples in
    production. ST’s Quality department must be contacted prior to any decision to use these engineering
    samples to run a qualification activity.

## Table 80. WLCSP49 - Example of PCB design rules (0.4 mm pitch)

```
Dimension Recommended values
```
```
Pitch 0.4
```
```
Dpad
```
```
260 μm max. (circular)
```
```
220 μm recommended
```
```
Dsm 300 μm min. (for 260 μm diameter pad)
```
```
PCB pad design Non-solder mask defined via underbump allowed.
```
```
MSv36161V1
```
#### E411CEB

#### Y WW

```
Product identification(1)
```
#### R

```
Ball 1
indentifier
```
```
Date code
```
```
Revision code
```

```
DS10314 Rev 8 129/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
### 7.3 UFQFPN48 package information (A0B9)

```
This UFQFPN is a 48-lead, 7 x 7 mm, 0.5 mm pitch, ultra thin fine pitch quad flat package.
```
## Figure 49. UFQFPN48 – Outline

1. Drawing is not to scale.
2. All leads/pads should also be soldered to the PCB to improve the lead/pad solder joint life.
3. There is an exposed die pad on the underside of the UFQFPN48 package. It is recommended to connect
    and solder this back-side pad to PCB ground.

```
D1
```
```
E1
```
```
EXPOSED PAD
```
```
E2
e
```
```
D2
```
```
PIN 1 idenfier
```
```
BOTTOM VIEW
```
```
L
```
```
A
A3
```
```
C
```
```
FRONT VIEW
```
```
DETAIL A
```
```
SEATING PLANE
```
```
LEADS COPLANARITY
```
```
dddC
```
```
SEATING PLANE
```
```
A1
```
```
A1 A
```
```
C
```
```
ddd C
PIN 1 IDENTIFIER
LASER MAKER AREA
```
```
E
```
```
D
TOP VIEW
```
```
A0B9_UFQFPN48_ME_V4
```

**Package information STM32F411xC STM32F411xE**

130/151 DS10314 Rev 8

## Figure 50. UFQFPN48 – Footprint example

1. Dimensions are expressed in millimeters.

## Table 81. UFQFPN48 – Mechanical data

```
Symbol
```
```
millimeters inches(1)
```
1. Values in inches are converted from mm and rounded to four decimal digits.

```
Min Typ Max Min Typ Max
```
```
A 0.500 0.550 0.600 0.0197 0.0217 0.0236
```
```
A1 0.000 0.020 0.050 0.0000 0.0008 0.0020
```
```
A3 - 0.152 - - 0.0060 -
```
```
b 0.200 0.250 0.300 0.0079 0.0098 0.0118
```
```
D(2)
```
2. Dimensions D and E do not include mold protrusion, not exceed 0.15 mm.

##### 6.900 7.000 7.100 0.2717 0.2756 0.2795

##### D1 5.400 5.500 5.600 0.2126 0.2165 0.2205

##### D2(3)

3. Dimensions D2 and E2 are not in accordance with JEDEC.

##### 5.500 5.600 5.700 0.2165 0.2205 0.2244

##### E(2) 6.900 7.000 7.100 0.2717 0.2756 0.2795

##### E1 5.400 5.500 5.600 0.2126 0.2165 0.2205

##### E2(3) 5.500 5.600 5.700 0.2165 0.2205 0.2244

```
e - 0.500 - - 0.0197 -
```
```
L 0.300 0.400 0.500 0.0118 0.0157 0.0197
```
```
ddd - - 0.080 - - 0.0031
```
```
7.30
```
```
7.30
```
```
0.20
```
```
0.30
```
```
0.55 0.50
5.80
```
```
6.20
```
```
6.20
```
```
5.60
```
```
5.60
```
```
5.80
```
```
0.75
A0B9_UFQFPN48_FP_V3
```
```
48
1
```
```
12
13 24
```
```
25
```
```
36
```
```
37
```

```
DS10314 Rev 8 131/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
### 7.4 LQFP64 package information (5W)

```
This LQFP is 64-pin, 10 x 10 mm low-profile quad flat package.
```
_Note: See list of notes in the notes section._

## Figure 51. LQFP64 - Outline(15).

```
D 1/4
```
```
E 1/4
```
```
(6)
```
```
aaa C A-B D
```
```
4x N/4 TIPS
```
```
bbb H A-B D 4x
```
```
(13)(N – 4)x e
```
```
0.05
```
```
A
```
```
A2 A1(12) b ddd C A-B D ccc
```
```
C
```
```
C
```
```
D
(5) (2)
```
```
(4)
D1
D(3)
```
```
D 1/4
```
```
E 1/4
```
```
(6)
```
```
1
2
3
(3) A B(3) (5)
(2)
E1 E
```
```
(Section A-A)A A
```
```
(4)
```
```
5W_LQFP64_ME_V1
```
```
(10) N
```
```
BOTTOM VIEW
```
```
TOP VIEW
```
```
SECTION A-A
```
```
GAUGE PLANE
```
```
B
```
```
BSECTION B-B
```
```
H
```
```
L
```
```
S
```
```
R1
R2
```
```
SECTION B-B
```
```
b
```
```
b1
```
```
c c1
```
```
WITH PLATING
```
```
BASE METAL
```
```
1
```
```
3
```
```
2
```
```
(L1)
```
```
(2)
```
```
0.25
```
```
(11)
```
```
(9)(11)
```
```
(11) (11)
```
```
(11)
```
```
(1)
```

**Package information STM32F411xC STM32F411xE**

132/151 DS10314 Rev 8

## Table 82. LQFP64 - Mechanical data

```
Symbol
```
```
millimeters inches(14)
```
```
Min Typ Max Min Typ Max
```
```
A - - 1.60 - - 0.0630
A1(12) 0.05 - 0.15 0.0020 - 0.0059
A2 1.35 1.40 1.45 0.0531 0.0551 0.0570
b(9)(11) 0.17 0.22 0.27 0.0 067 0.0087 0.0106
b1(11) 0.17 0.20 0.23 0.0 067 0.0079 0.0091
c(11) 0.09 - 0.20 0.0035 - 0.0079
c1(11) 0.09 - 0.16 0.0035 - 0.0063
D(4) 12.00 BSC 0.4724 BSC
D1(2)(5) 10.00 BSC 0.3937 BSC
E(4) 12.00 BSC 0.4724 BSC
E1(2)(5) 10.00 BSC 0.3937 BSC
e 0.50 BSC 0.1970 BSC
L 0.45 0.60 0.75 0.0177 0.0236 0.0295
L1 1.00 REF 0.0394 REF
N(13) 64
θ 0° 3.5° 7° 0° 3.5° 7°
θ1 0° - - 0° - -
θ2 10° 12° 14° 10° 12° 14°
θ3 10° 12° 14° 10° 12° 14°
R1 0.08 - - 0.0031 - -
R2 0.08 - 0.20 0.0031 - 0.0079
S 0.20 - - 0.0079 - -
aaa(1) 0.20 0.0079
bbb(1) 0.20 0.0079
ccc(1) 0.08 0.0031
ddd(1) 0.08 0.0031
```

```
DS10314 Rev 8 133/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
**Notes:**

1. Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2. The Top package body size may be smaller than the bottom package size by as much
    as 0.15 mm.
3. Datums A-B and D to be determined at datum plane H.
4. To be determined at seating datum plane C.
5. Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
    or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
    dimensions including mold mismatch.
6. Details of pin 1 identifier are optional but must be located within the zone indicated.
7. All Dimensions are in millimeters.
8. No intrusion allowed inwards the leads.
9. Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
    not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
    Dambar cannot be located on the lower radius or the foot. Minimum space between
    protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
    from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
    body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.

## Figure 52. LQFP64 - Footprint example

1. Dimensions are expressed in millimeters.

```
48
```
(^4932)
64 17
1 16
1.20
0.30
33
10.30
12.70
10.30
0.5
7.80
12.70 5W_LQFP64_FP_V2


**Package information STM32F411xC STM32F411xE**

134/151 DS10314 Rev 8

### 7.5 LQFP100 package information (1L)

```
This LQFP is 100 lead, 14 x 14 mm low-profile quad flat package.
```
_Note: See list of notes in the notes section._

## Figure 53. LQFP100 - Outline(15).

```
D1/4
```
```
E1/4
```
```
4x N/4 TIPS
aaaCA-BD bbb H A-B4x D
```
```
(N-4) x e
```
```
A
0.05
A2 A1 b aaa C A-BD cccC
```
```
C
```
```
D
D1
D
N
```
```
A
```
(^12)
3
SECTION A-A
A A
B
E1 E
SECTION A-A
GAUGE PLANE
B
BSECTION B-B
H
E1/4
D1/4
L
S
R1
R2
SECTION B-B
b
b1
c c1
WITH PLATING
BASE METAL
TOP VIEW
SIDE VIEW
BOTTOM VIEW
1L_LQFP100_ME_V3
(6)
(6)
(10)
ș 2 ș
ș
ș
(13)
(12)
(2)(5)
(4)
(4)
(5)
(2)
(3)
(L1)
(9)(11)
(11)
(11)
(11)
(2)
(1)(11)


```
DS10314 Rev 8 135/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
## Table 83. LQFP100 - Mechanical data

```
Symbol
```
```
millimeters inches(14)
```
```
Min Typ Max Min Typ Max
```
```
A - 1.50 1.60 - 0.0590 0.0630
```
```
A1(12) 0.05 - 0.15 0.0019 - 0.0059
```
```
A2 1.35 1.40 1.45 0.0531 0.0551 0.0570
```
```
b(9)(11) 0.17 0.22 0.27 0.0 067 0.0087 0.0106
```
```
b1(11) 0.17 0.20 0.23 0.0 067 0.0079 0.0090
```
```
c(11) 0.09 - 0.20 0.0035 - 0.0079
```
```
c1(11) 0.09 - 0.16 0.0035 - 0.0063
```
```
D(4) 16.00 BSC 0.6299 BSC
```
```
D1(2)(5) 14.00 BSC 0.5512 BSC
```
```
E(4) 16.00 BSC 0.6299 BSC
```
```
E1(2)(5) 14.00 BSC 0.5512 BSC
```
```
e 0.50 BSC 0.0197 BSC
```
```
L 0.45 0.60 0.75 0.177 0.0236 0.0295
```
```
L1(1)(11) 1.00 - 0.0394 -
```
```
N(13) 100
```
```
θ 0° 3.5° 7° 0° 3.5° 7°
```
```
θ1 0° - - 0° - -
```
```
θ2 10° 12° 14° 10° 12° 14°
```
```
θ3 10° 12° 14° 10° 12° 14°
```
```
R1 0.08 - - 0.0031 - -
```
```
R2 0.08 - 0.20 0.0031 - 0.0079
```
```
S 0.20 - - 0.0079 - -
```
```
aaa(1) 0.20 0.0079
```
```
bbb(1) 0.20 0.0079
```
```
ccc(1) 0.08 0.0031
```
```
ddd(1) 0.08 0.0031
```

**Package information STM32F411xC STM32F411xE**

136/151 DS10314 Rev 8

**Notes:**

1. Dimensioning and tolerancing schemes conform to ASME Y14.5M-1994.
2. The Top package body size may be smaller than the bottom package size by as much
    as 0.15 mm.
3. Datums A-B and D to be determined at datum plane H.
4. To be determined at seating datum plane C.
5. Dimensions D1 and E1 do not include mold flash or protrusions. Allowable mold flash
    or protrusions is “0.25 mm” per side. D1 and E1 are Maximum plastic body size
    dimensions including mold mismatch.
6. Details of pin 1 identifier are optional but must be located within the zone indicated.
7. All Dimensions are in millimeters.
8. No intrusion allowed inwards the leads.
9. Dimension “b” does not include dambar protrusion. Allowable dambar protrusion shall
    not cause the lead width to exceed the maximum “b” dimension by more than 0.08 mm.
    Dambar cannot be located on the lower radius or the foot. Minimum space between
    protrusion and an adjacent lead is 0.07 mm for 0.4 mm and 0.5 mm pitch packages.
10. Exact shape of each corner is optional.
11. These dimensions apply to the flat section of the lead between 0.10 mm and 0.25 mm
    from the lead tip.
12. A1 is defined as the distance from the seating plane to the lowest point on the package
    body.
13. “N” is the number of terminal positions for the specified body size.
14. Values in inches are converted from mm and rounded to 4 decimal digits.
15. Drawing is not to scale.

## Figure 54. LQFP100 - Footprint example

1. Dimensions are expressed in millimeters.

```
75 51
```
(^76) 0.5 50
0.3
16.7 14.3
100 26
12.3
25
1.2
16.7
1
1L_LQFP100_FP_V1


```
DS10314 Rev 8 137/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
### 7.6 UFBGA100 package information (A0C2)

```
This UFBGA is a 100-ball, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid array package.
```
_Note: See list of notes in the notes section._

## Figure 55. UFBGA100 - Outline(13)

```
A0C2_UFBGA_ME_V8
```
```
e
```
```
D
```
```
A
```
```
Øb (N balls)
```
```
E
```
```
TOP VIEW
```
```
BOTTOM VIEW
```
```
1
```
```
e
```
```
A
```
```
A
```
```
B
```
```
D1
```
```
E1
```
```
eee CA B
fff
```
```
Ø
Ø
```
```
M
MC
```
```
A1 ball pad
corner
```
```
B
```
```
C
```
```
E
D
```
```
F
```
```
G
```
```
H
```
```
J
```
```
K
```
```
L
```
```
M
```
```
245 7 3 6891110
```
```
Seating plane
```
```
C A1 A2
ddd
```
```
ccc C
```
```
C
```
```
Detail A
Solder balls
```
```
Mold resin
```
```
Substrate
```
```
(8)
```
```
(DATUM A)
```
```
(DATUM B)
```
```
aaaC
(4X)
```
```
(9)
```
```
SIDE VIEW
```
```
DETAIL A
```
```
C
```
```
12
```
```
SE
```
```
SD
```
```
A1 ball pad
corner
```

**Package information STM32F411xC STM32F411xE**

138/151 DS10314 Rev 8

**Notes:**

1. Dimensioning and tolerancing schemes conform to ASME Y14.5M-2009 apart
    European projection.
2. UFBGA stands for ulta profile fine pitch ball grid array: 0.50 mm < A ≤ 0.65 mm / fine
    pitch e < 1.00 mm.
3. The profile height, A, is the distance from the seating plane to the highest point on the
    package. It is measured perpendicular to the seating plane.
4. A1 is defined as the distance from the seating plane to the lowest point on the package
    body.
5. Dimension b is measured at the maximum diameter of the terminal (ball) in a plane
    parallel to primary datum C.
6. BSC stands for BASIC dimensions. It corresponds to the nominal value and has no
    tolerance. For tolerances refer to form and position table. On the drawing these
    dimensions are framed.
7. Primary datum C is defined by the plane established by the contact points of three or
    more solder balls that support the device when it is placed on top of a planar surface.
8. The terminal (ball) A1 corner must be identified on the top surface of the package by
    using a corner chamfer, ink or metalized markings, or other feature of package body or

## Table 84. UFBGA100 - Mechanical data

```
Symbol
```
```
millimeters(1) inches(12)
```
```
Min. Typ. Max. Min. Typ. Max.
```
```
A(2)(3) - - 0.60 - - 0.0236
```
```
A1(4) 0.05 - - 0.0020 - -
```
```
A2 - 0.43 - - 0.0169 -
```
```
b(5) 0.23 0.28 0.33 0.0090 0.0110 0.0130
```
```
D(6) 7.00 BSC 0.2756 BSC
```
```
D1 5.50 BSC 0.2165 BSC
```
```
E 7.00 BSC 0.2756 BSC
```
```
E1 5.50 BSC 0.2165 BSC
```
```
e(9) 0.50 BSC 0.0197 BSC
```
```
N(11) 100
```
```
SD(12) 0.25 BSC 0.0098 BSC
```
```
SE(12) 0.25 BSC 0.0098 BSC
```
```
aaa 0.15 0.0059
```
```
ccc 0.20 0.0079
```
```
ddd 0.08 0.0031
```
```
eee 0.15 0.0059
```
```
fff 0.05 0.0020
```

```
DS10314 Rev 8 139/151
```
**STM32F411xC STM32F411xE Package information**

```
141
```
```
integral heat slug. A distinguish feature is allowable on the bottom surface of the
package to identify the terminal A1 corner. Exact shape of each corner is optional.
```
9. e represents the solder ball grid pitch.
10. N represents the total number of balls on the BGA.
11. Basic dimensions SD and SE are defined with respect to datums A and B. It defines the
    position of the centre ball(s) in the outer row or column of a fully populated matrix.
12. Values in inches are converted from mm and rounded to 4 decimal digits.
13. Drawing is not to scale.

## Figure 56. UFBGA100 - Footprint example

## Table 85. UFBGA100 - Example of PCB design rules (0.5 mm pitch BGA)

```
Dimension Values
```
```
Pitch 0.50 mm
```
```
Dpad 0.280 mm
```
```
Dsm
```
```
0.370 mm typ. (depends on the solder mask
registration tolerance)
```
```
Stencil opening 0.280 mm
```
```
Stencil thickness Between 0.100 mm and 0.125 mm
```
```
BGA_WLCSP_FT_V1
```
```
Dsm
```
```
Dpad
```

**Package information STM32F411xC STM32F411xE**

140/151 DS10314 Rev 8

### 7.7 Thermal characteristics

```
The maximum chip junction temperature (TJmax) must never exceed the values given in
Table 14: General operating conditions on page 59.
```
```
The maximum chip-junction temperature, TJ max., in degrees Celsius, may be calculated
using the following equation:
```
```
TJ max = TA max + (PD max x ΘJA)
```
```
Where:
```
- TA max is the maximum ambient temperature in °C,
•ΘJA is the package junction-to-ambient thermal resistance, in °C/W,
- PD max is the sum of PINT max and PI/O max (PD max = PINT max + PI/Omax),
- PINT max is the product of IDD and VDD, expressed in Watts. This is the maximum chip
    internal power.

```
PI/O max represents the maximum power dissipation on output pins where:
PI/O max = Σ (VOL × IOL) + Σ((VDD – VOH) × IOH),
```
```
taking into account the actual VOL / IOL and VOH / IOH of the I/Os at low and high level in the
application.
```
#### 7.7.1 Reference document

```
JESD51-2 Integrated Circuits Thermal Test Method Environment Conditions - Natural
Convection (Still Air). Available from http://www.jedec.org.
```
## Table 86. Package thermal characteristics

```
Symbol Parameter Value Unit
```
##### ΘJA

```
Thermal resistance junction-ambient
UFQFPN48^32
```
##### °C/W

```
Thermal resistance junction-ambient
WLCSP49^51
```
```
Thermal resistance junction-ambient
LQFP64^47
```
```
Thermal resistance junction-ambient
LQFP100^43
```
```
Thermal resistance junction-ambient
UFBGA100^62
```

```
DS10314 Rev 8 141/151
```
**STM32F411xC STM32F411xE Ordering information**

```
141
```
## 8 Ordering information

```
For a list of available options (memory, package, and so on) or for further information on any
```
```
aspect of this device, contact the nearest ST sales office.
```
## Table 87. Ordering information scheme

```
Example: STM32 F 411 C E Y 6 TR
```
```
Device family
```
```
STM32 = Arm®-based 32-bit microcontroller
```
```
Product type
```
```
F = General-purpose
```
```
Device subfamily
```
```
411 = 411 family
```
```
Pin count
```
```
C = 48/49 pins
```
```
R = 64 pins
```
```
V = 100 pins
```
```
Flash memory size
```
```
C = 256 Kbytes of flash memory
```
```
E = 512 Kbytes of flash memory
```
```
Package
```
```
H = UFBGA
```
```
T = LQFP
```
```
U = UFQFPN
```
```
Y = WLCSP
```
```
Temperature range
```
```
6 = Industrial temperature range, - 40 to 85 °C
```
```
7 = Industrial temperature range, - 40 to 105 °C
```
```
3 = Industrial temperature range, - 40 to 125 °C
```
```
Packing
```
```
TR = tape and reel
```
```
No character = tray or tube
```

**Recommendations when using the internal reset OFF STM32F411xC STM32F411xE**

142/151 DS10314 Rev 8

## Appendix A Recommendations when using the internal reset OFF

```
When the internal reset is OFF, the following integrated features are no longer supported:
```
- The integrated power-on-reset (POR)/power-down reset (PDR) circuitry is disabled.
- The brownout reset (BRO) circuitry must be disabled. By default BOR is OFF.
- The embedded programmable voltage detector (PVD) is disabled.
- VBAT functionality is no more available and VBAT pin should be connected to VDD.

### A.1 Operating conditions

## Table 88. Limitations depending on the operating power supply range

```
Operating
power supply
range
```
##### ADC

```
operation
```
```
Maximum
flash memory
access
frequency
with no wait
state
(fFlashmax)
```
```
Maximum
flash memory
access
frequency
with no wait
states(1) (2)
```
1. Applicable only when the code is executed from flash memory. When the code is executed from RAM, no
    wait state is required.
2. Thanks to the ART accelerator and the 128-bit flash memory, the number of wait states given here does not
    impact the execution speed from flash memory since the ART accelerator allows to achieve a performance
    equivalent to 0 wait state program execution.

```
I/O operation
```
```
Possible flash
memory
operations
```
```
VDD = 1.7 to
2.1 V(3)
```
3. VDD/VDDA minimum value of 1.7 V, with the use of an external power supply supervisor (refer to
    _Section 3.15.1: Internal reset ON_ ).

```
Conversion
time up to
1.2 Msps
```
```
20 MHz(4)
```
4. Prefetch is not available. Refer to AN3430 application note for details on how to adjust performance and
    power.

```
100 MHz with
6 wait states
```
```
No I/O
compensation
```
```
8-bit erase and
program
operations only
```

```
DS10314 Rev 8 143/151
```
**STM32F411xC STM32F411xE Application block diagrams**

```
146
```
## Appendix B Application block diagrams

### B.1 USB OTG Full Speed (FS) interface solutions

## Figure 57. USB controller configured as peripheral-only and used in Full-Speed mode

1. The external voltage regulator is only needed when building a VBUS powered device.

## Figure 58. USB controller configured as host-only and used in Full-Speed mode.

1. The current limiter is required only if the application has to support a VBUS powered device. A basic power
    switch can be used if 5V are available on the application board.

```
MS35538V1
```
##### V

```
STM32F411xCxE
```
##### OSC_IN

##### OSC_OUT

##### PA11

##### PA12

```
DD
5 V to VDD
Voltage
regulator (1)
```
##### VBUS

##### DM

##### DP

##### VSS

```
USB Std-B connector
```
```
MS35539V1
```
```
STM32F411xCxE
```
```
GPIO
```
```
GPIO+IRQ
```
```
PA11
PA12
```
```
OSC_IN
```
```
OSC_OUT
```
```
EN
```
```
Overcurrent
```
```
VDD
```
```
Current limiter
power switch(1)
```
```
5 V Power
```
```
VBUS
DM
DP
```
```
VSS
USB Std-A connector
```

**Application block diagrams STM32F411xC STM32F411xE**

144/151 DS10314 Rev 8

## Figure 59. USB controller configured in dual mode and used in Full-Speed mode

1. The external voltage regulator is only needed when building a VBUS powered device.
2. The current limiter is required only if the application has to support a VBUS powered device. A basic power
    switch can be used if 5 V are available on the application board.
3. The ID pin is required in dual role only.

```
MS35540V1
```
```
VDD
```
```
PA12
PA10
```
```
(2)
```
```
PA9
PA11
```
```
GPIO+IRQ
```
```
GPIO
```
```
STM32F411xCxE
```
```
OSC_IN
```
```
OSC_OUT
```
```
VDD
```
```
EN
```
```
Overcurrent
```
```
Current limiter
power switch
```
```
5V power
```
```
5 V to VDD
voltage
regulator(1)
```
```
USBnicro-AB connector
```
```
VSS
```
```
ID(3)
```
```
DP
```
```
DM
```
```
VBUS
```

```
DS10314 Rev 8 145/151
```
**STM32F411xC STM32F411xE Application block diagrams**

```
146
```
### B.2 Sensor Hub application example.

## Figure 60. Sensor Hub application example

```
Accelerometer
Gyroscope
```
```
Magnetometer
```
```
Pressure
```
```
Ambient light
```
```
Proximity
```
```
Micro
```
```
SPI
```
```
Temperature/Humidity
```
```
I2S
```
```
SCL
SDA
```
```
PB6/PB10/PA8
PB7/PB9/PB4
SLK
DATA
```
```
PB13
PB15
```
```
PA 4NSS
SCK
MISO
MOSI
```
```
PA 5
PA 6
PA 7
```
```
PA 1/PA 3
```
```
TX
RX
```
```
PA 9
PA 10
```
```
UART
```
```
I2C
```
```
ADC
PC15
```
```
OSC 32k PC14
```
```
NRST
```
```
SWDIO
```
```
PB3
```
```
PA 14
```
```
PA 13
SWCLK
SWO
```
```
JTAG
```
```
PDRON
```
```
VDD
```
```
15x GPIO GPIO
```
```
Up to 10 ADC inputs possible for the 48 and 49 pins package
```
```
BOOT0
```
```
10k
```
```
MS35548V1
```
```
STM32F411xE
48- and 49-pin package
```
```
HOST
```

**Application block diagrams STM32F411xC STM32F411xE**

146/151 DS10314 Rev 8

### B.3 Batch Acquisition Mode (BAM) example

```
Data is transferred through the DMA from interfaces into the internal SRAM while the rest of
the MCU is set in low power mode.
```
- Code execution from RAM before switching off the Flash.
- Flash is set in power down and flash interface (ART™ accelerator) clock is stopped.
- The clocks are enabled only for the required interfaces.
- MCU core is set in sleep mode (core clock stopped waiting for interrupt).
- Only the needed DMA channels are enabled and running.

## Figure 61. Batch Acquisition Mode (BAM) example

```
Temperature/Humidity
```
```
SCL
SDA
```
```
PB6/PB10/PA8
PB7/PB9/PB4
```
```
SLK
DATA
```
```
PB13
PB15
```
```
PA 4NSS
SCK
MISO
MOSI
```
```
PA 5
PA 6
PA 7
```
```
PA 1/PA 3
```
```
TX
RX
```
```
PA 9
PA 10
```
```
PC15
```
```
OSC 32k PC14
```
```
NRST
```
```
SWDIO
```
```
PB3
```
```
PA 14
```
```
PA 13
SWCLK
SWO
```
```
JTAG
```
```
PDRON
```
```
VDD
```
```
15x GPIO GPIO
```
```
Up to 10 ADC inputs possible for the 48 and 49 pins package
```
```
10k BOOT0
```
```
MS35549V1
```
```
STM32F411xE
48- and 49-pin package
```
```
1x 12-bit ADC
10 channels/2 Msps
```
```
5x SPI or
5x I2S
(2x full duplex)
```
```
3x I2C
CORTEX M4
CPU + MPU
+ FPU
100 MHz
```
```
512 kB Flash
ART
```
```
128 kB SRAM
```
```
DMA
```
```
Legend: Low-power part Active part
```
```
HOST
```
```
Proximity
```
```
Micro
```
```
Ambient light
```
```
Magnetometer
```
```
Accelerometer
Gyroscope
```
```
I2C Pressure
```
```
I2S
```
```
UART
```
```
SPI
```
```
ADC
```

```
DS10314 Rev 8 147/151
```
**STM32F411xC STM32F411xE Important security notice**

```
147
```
## 9 Important security notice

```
The STMicroelectronics group of companies (ST) places a high value on product security,
which is why the ST product(s) identified in this documentation may be certified by various
security certification bodies and/or may implement our own security measures as set forth
herein. However, no level of security certification and/or built-in security measures can
guarantee that ST products are resistant to all forms of attacks. As such, it is the
responsibility of each of ST's customers to determine if the level of security provided in an
ST product meets the customer needs both in relation to the ST product alone, as well as
when combined with other components and/or software for the customer end product or
application. In particular, take note that:
```
- ST products may have been certified by one or more security certification bodies, such
    as Platform Security Architecture (www.psacertified.org) and/or Security Evaluation
    standard for IoT Platforms (www.trustcb.com). For details concerning whether the ST
    product(s) referenced herein have received security certification along with the level
    and current status of such certification, either visit the relevant certification standards
    website or go to the relevant product page on [http://www.st.com](http://www.st.com) for the most up to date
    information. As the status and/or level of security certification for an ST product can
    change from time to time, customers should re-check security certification status/level
    as needed. If an ST product is not shown to be certified under a particular security
    standard, customers should not assume it is certified.
- Certification bodies have the right to evaluate, grant and revoke security certification in
    relation to ST products. These certification bodies are therefore independently
    responsible for granting or revoking security certification for an ST product, and ST
    does not take any responsibility for mistakes, evaluations, assessments, testing, or
    other activity carried out by the certification body with respect to any ST product.
- Industry-based cryptographic algorithms (such as AES, DES, or MD5) and other open
    standard technologies which may be used in conjunction with an ST product are based
    on standards which were not developed by ST. ST does not take responsibility for any
    flaws in such cryptographic algorithms or open technologies or for any methods which
    have been or may be developed to bypass, decrypt or crack such algorithms or
    technologies.
- While robust security testing may be done, no level of certification can absolutely
    guarantee protections against all attacks, including, for example, against advanced
    attacks which have not been tested for, against new or unidentified forms of attack, or
    against any form of attack when using an ST product outside of its specification or
    intended use, or in conjunction with other components or software which are used by
    customer to create their end product or application. ST is not responsible for resistance
    against such attacks. As such, regardless of the incorporated security features and/or
    any information or support that may be provided by ST, each customer is solely
    responsible for determining if the level of attacks tested for meets their needs, both in
    relation to the ST product alone and when incorporated into a customer end product or
    application.
- All security features of ST products (inclusive of any hardware, software,
    documentation, and the like), including but not limited to any enhanced security
    features added by ST, are provided on an "AS IS" BASIS. AS SUCH, TO THE EXTENT
    PERMITTED BY APPLICABLE LAW, ST DISCLAIMS ALL WARRANTIES, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, unless the
    applicable written and signed contract terms specifically provide otherwise.


**Revision history STM32F411xC STM32F411xE**

148/151 DS10314 Rev 8

## Revision history

## Table 89. Document revision history

```
Date Revision Changes
```
```
19-Jun-2014 1 Initial release.
```
```
10-Sep-2014 2
```
```
Introduced the BAM feature in Features , Section 2: Description ., and Section 3.3: Batch
Acquisition mode (BAM).
Updated Section 3.5: Embedded flash memory , Section 3.14: Power supply schemes
and Section 3.18: Low-power modes , Section 3.20.2: General-purpose timers (TIMx)
and Section 3.30: Temperature sensor.
Modified Table 8: STM32F411xC/xE pin definitions , Table 9: Alternate function mapping
and APB2 in Table 10: STM32F411xC/xE register boundary addresses.
Modified Table 34: Low-power mode wakeup timings(1) , Table 20: Typical and maximum
current consumption, code with data processing (ART accelerator disabled) running
from SRAM - VDD = 1.7 V , Table 21: Typical and maximum current consumption, code
with data processing (ART accelerator disabled) running from SRAM - VDD = 3.6 V ,
Table 25: Typical and maximum current consumption in run mode, code with data
processing (ART accelerator enabled with prefetch) running from flash memory - VDD =
3.6 V , Table 26: Typical and maximum current consumption in Sleep mode - VDD = 3.6 V
and Table 58: I^2 C characteristics and Figure 33: I^2 C bus AC waveforms and
measurement circuit.
Added Figure 21: Low-power mode wakeup , Section Appendix A: Recommendations
when using the internal reset OFF and Section Appendix B: Application block diagrams.
```
```
27-Nov-2014 3
```
```
Changed datasheet status to Production Data.
Updated Table 31: Typical and maximum current consumptions in VBAT mode.
Section : On-chip peripheral current consumption : changed HCLK frequency and
updated DMA1 and DMA2 current consumption in Table 33: Peripheral current
consumption.
Updated Table 55: I/O AC characteristics.
Updated THD in Table 69: ADC dynamic accuracy at fADC = 18 MHz - limited test
conditions and Table 70: ADC dynamic accuracy at fADC = 36 MHz - limited test
conditions.
Updated Table 55: I/O AC characteristics.
Updated Figure 48: WLCSP49 - 49-ball, 2.999 x 3.185 mm, 0.4 mm pitch wafer level
chip scale package outline and Figure 48: WLCSP49 marking (package top view).
Added Figure 49: WLCSP49 - 49-ball, 2.999 x 3.185 mm, 0.4 mm pitch wafer level chip
scale recommended footprint and Table 82: WLCSP49 recommended PCB design rules
(0.4 mm pitch).
Updated Figure 7.4: LQFP64 package information (5W) , Figure 57: LQFP64 marking
example (package top view) , Figure 61: LQPF100 marking example (package top view) ,
and Figure 91: UFBGA100 - 100-ball, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid
array package mechanical data.
```
```
04-Feb-2015 4
```
```
Added VPP alternate function for BOOT0 in Table 8: STM32F411xC/xE pin definitions.
Added TC inputs in Table 11: Voltage characteristics , Table 12: Current characteristics ,
Table 14: General operating conditions , Table 53: I/O static characteristics and
Figure 30: FT/TC I/O input characteristics.
Updated VESD(CDM) in Table 50: ESD absolute maximum ratings.
A3 minimum and maximum values removed in Table 83: UFBGA100 - 100-ball, 7 x 7
mm, 0.50 mm pitch, ultra fine pitch ball grid array package mechanical data.
```

```
DS10314 Rev 8 149/151
```
**STM32F411xC STM32F411xE Revision history**

```
150
```
```
21-Nov-2016 5
```
```
Updated:
```
- _Features_
- _Figure 1: Compatible board design for LQFP100 package_
- _Figure 2: Compatible board design for LQFP64 package_
- _Figure 3: STM32F411xC/xE block diagram_
- _Figure 22: High-speed external clock source AC timing diagram_
- _Figure 23: Low-speed external clock source AC timing diagram_
- _Figure 33: I_^2 _C bus AC waveforms and measurement circuit_
- _Figure 64: UFBGA100 - 100-ball, 7 x 7 mm, 0.50 mm pitch, ultra fine pitch ball grid_
    _array package outline_
- _Table 2: STM32F411xC/xE features and peripheral counts_
- _Table 8: STM32F411xC/xE pin definitions_
- _Table 13: Thermal characteristics_
- _Table 14: General operating conditions_
–From _Table 20: Typical and maximum current consumption, code with data processing_
    _(ART accelerator disabled) running from SRAM - VDD = 1.7 V_ to _Table 31: Typical and_
    _maximum current consumptions in VBAT mode_
- _Table 35: High-speed external user clock characteristics_
- _Table 36: Low-speed external user clock characteristics_
- _Table 39: HSI oscillator characteristics_
- _Table 47: Flash memory endurance and data retention_
- _Table 51: Electrical sensitivities_
- _Table 53: I/O static characteristics_
- _Table 76: Dynamic characteristics: SD / MMC characteristics_
- _Table 87: Ordering information scheme_
Added:
- _To optimize the power consumption the flash memory can also be switched off in Run_
    _or in Sleep mode (see Section 3.18: Low-power modes). Two modes are available:_
    _Flash in Stop mode or in DeepSleep mode (trade off between power saving and_
    _startup time, see Table 34: Low-power mode wakeup timings(1)). Before disabling the_
    _flash memory, the code must be executed from the internal RAM. One-time_
    _programmable bytes_
- _Table 86: Package thermal characteristics_

```
05-Dec-2016 6
```
```
Updated:
```
- _Table 27: Typical and maximum current consumptions in Stop mode - VDD = 1.7 V_
- _Table 28: Typical and maximum current consumption in Stop mode - VDD=3.6 V_
- _Table 29: Typical and maximum current consumption in Standby mode - VDD= 1.7 V_
- _Table 30: Typical and maximum current consumption in Standby mode - VDD= 3.6 V_

```
Table 89. Document revision history
```
```
Date Revision Changes
```

**Revision history STM32F411xC STM32F411xE**

150/151 DS10314 Rev 8

```
14-Dec-2017 7
```
```
Updated:
```
- _Table 27: Typical and maximum current consumptions in Stop mode - VDD = 1.7 V_
- _Table 28: Typical and maximum current consumption in Stop mode - VDD=3.6 V_
- _Table 29: Typical and maximum current consumption in Standby mode - VDD= 1.7 V_
- _Table 30: Typical and maximum current consumption in Standby mode - VDD= 3.6 V_

```
29-Jan-2024 8
```
```
Updated:
–Features
```
_- Description
- Table 9: Alternate function mapping
- Table 9: STM32F411xC/xE WLCSP49 pinout
- Section 7: Package information
- Section 7.2: WLCSP49 package information (A0ZV)
- Section 7.3: UFQFPN48 package information (A0B9)
- Section 7.5: LQFP100 package information (1L)
- Section 7.6: UFBGA100 package information (A0C2)_
Added:
_- Application_
- _Section 7.1: Device marking
- Section 9: Important security notice_
Removed all markings except the WLCSP marking.

```
Table 89. Document revision history
```
```
Date Revision Changes
```

```
DS10314 Rev 8 151/151
```
**STM32F411xC STM32F411xE**

```
151
```
```
IMPORTANT NOTICE – READ CAREFULLY
```
STMicroelectronics NV and its subsidiaries (“ST”) reserve the right to make changes, corrections, enhancements, modifications, and
improvements to ST products and/or to this document at any time without notice. Purchasers should obtain the latest relevant information on
ST products before placing orders. ST products are sold pursuant to ST’s terms and conditions of sale in place at the time of order
acknowledgment.

Purchasers are solely responsible for the choice, selection, and use of ST products and ST assumes no liability for application assistance or
the design of purchasers’ products.

No license, express or implied, to any intellectual property right is granted by ST herein.

Resale of ST products with provisions different from the information set forth herein shall void any warranty granted by ST for such product.

ST and the ST logo are trademarks of ST. For additional information about ST trademarks, refer to [http://www.st.com/trademarks.](http://www.st.com/trademarks.) All other product
or service names are the property of their respective owners.

Information in this document supersedes and replaces information previously supplied in any prior versions of this document.

```
© 2024 STMicroelectronics – All rights reserved
```

