# Shorty_AVR_DD

## What is it for?

Shorty is short circuit finder, usable for checking PCB, for example. Where normal digital multimeters do not have enough of resolution to differentiate low PCB resistance in order of tens of miliohms to allow exact short circuit location, shorty comes to help.

## Design origin and adoptions 

This is a fork of the original version from kripton2035. The new development aim to meet my own requirements and needs.
- Use of AVR DD series (AVR32DD20) instead of Arduino Nano / Pro Mini with Atmega328P, which provides:
  - powerful AVR core with HW multiplier
  - more advanced peripherals,
  - memory (32kB Flash, 4kB SRAM),
  - higher clock speed (up to 24 MHz),
  - low power consumption,
  - Low voltage supply operation (1.8 V - 5.5 V)
- Update of hardware:
   - Another Zero-drift OPamp used (AD8551) due to lack of availbility of the original one, 0.1% resistors used
   - 3.3V supply only for all components,
   - re-evaluated equation to calcuate Rx from ADC data (thanks to kripton2035 for his support!)

# Original idea links

- https://hackaday.io/project/3635-shorty-short-circuit-finder
- https://www.eevblog.com/forum/testgear/finding-short-on-motherboards-with-a-shorty-(with-display)
- http://kripton2035.free.fr/Continuity%20Meters/shorty-with-disp.html

# Project status

## Software 
- Work in progress (WIP)
- partly tested, current status:
  - Display -> OK
  - 18bit ADC -> OPEN
  - ADC -> OK
  - Calibration -> OPEN
 
## Hardware
- Current implementation -> only perf board with Xiao and Display
- Kicad Design -> started
- Housing -> OPEN

 # Compilation from Sources

Code has been developed with VSCode and platformIO plugin

# Known limitations

Even though the Xiao is well supported by arduino core there seems to be some functions to be checked for correct operation:
- tone() -> basic function seems to be ok, jitter observed on oscilloscope. Reason unknown so far and also if the jitter has any affect in practise.
- noTone() -> do not use in setup() routine otherwise software get stuck

# Final remarks

- Code is provided **AS IS**, so no warranties.
