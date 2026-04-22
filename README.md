# Shorty_AVR_DD

## What is it for?

Shorty is short circuit finder, usable for checking PCB, for example. Where normal digital multimeters do not have enough of resolution to differentiate low PCB resistance in order of tens of miliohms to allow exact short circuit location, shorty comes to help.

## Design origin and adoptions 

This is a fork of the original version from kripton2035. The new development aim to meet my own requirements and needs.
- Use of AVR DD series (AVR32DD20) instead of Arduino Nano / Pro Mini with Atmega328P, which provides:
  - powerful AVR core with HW multiplier
  - more advanced peripherals (even though not really needed),
  - memory (32kB Flash, 4kB SRAM) - ok, flash memory is identical to Atmega328P
  - higher clock speed (up to 24 MHz),
  - low power consumption,
  - Low voltage supply operation (1.8 V - 5.5 V)
  - Since there is no real reason to change the micro controller from HW requirements point of view, the main motivation was to learn coding the new AVR Dx series. 
- Update of hardware:
   - Another Zero-drift OPamp used (AD8551) due to lack of availbility of the original one, 0.1% resistors used
   - 3.3V supply only for all components,
   - re-evaluated equation to calcuate Rx from ADC data (thanks to kripton2035 for his support!)

# Original idea links

- https://hackaday.io/project/3635-shorty-short-circuit-finder
- https://www.eevblog.com/forum/testgear/finding-short-on-motherboards-with-a-shorty-(with-display)
- http://kripton2035.free.fr/Continuity%20Meters/shorty-with-disp.html
- https://gitlab.com/jdobry/shortypen

# Project status

## Software 
- Work in progress (WIP)
- partly tested, current status:
  - Display -> OK
  - 18bit ADC -> OK
  - ADC -> OK
  - Battery status -> OK
  - Calibration -> OPEN
 
## Hardware
- Current implementation -> ready with AVR Dx series
- Kicad Design -> completed
- Housing -> commercial available hand held housing used.
- Complete build:
  - not finished
  - some mechanics work to do and
  - I need to order a small 4 pin connector

 # Compilation from Sources

Code has been developed with VSCode and platformIO plugin. Since the AVR32DD14/ AVR32DD20 was not directly supported from the standard tool chain at the time of writing . It needs a newer toolchain version with AVR32DD14/ AVR32DD20 support.
Refer to [AVR32DD14 support](https://community.platformio.org/t/avr32dd14-support/45906).

Required steps:
- Board support is added in the form of JSON files in the boards/ folder of the platform
- add tool chain in platformio.ini:
  platform_packages =
    toolchain-atmelavr@symlink://<Path_to_Tool_chain>/toolchain-atmelavr-windows

# Known limitations
- None

# Final remarks

- Code is provided **AS IS**, so no warranties.
