# Shorty_Xiao

This is a fork of the original version from kripton2035. The new development aim to meet my own requirements and needs.
- Use of Seeeduino Xiao (SAMD21G18) instead of Arduino Nano / Pro Mini with Atmega328P, which provides:
  - powerful ARM Cortex M0+ core 
  - more advanced peripherals,
  - more memory and
  - higher clock speed
  - low power consumption
  - Low voltage supply operation
- Update of hardware:
   - A fully integrated differential amplifier with gain 50 used. Pro: matched internal resistor network for higher accuracy
   - re-evaluated equation for Rx (thanks to kripton2035 for his support!)
   - 3.3V supply only for all components, since SAMD21G18 has no 5V tolerant inputs


# Original idea links

- https://hackaday.io/project/3635-shorty-short-circuit-finder
- https://www.eevblog.com/forum/testgear/finding-short-on-motherboards-with-a-shorty-(with-display)
- http://kripton2035.free.fr/Projects/proj-shorty.html](http://kripton2035.free.fr/Continuity%20Meters/shorty-with-disp.html

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
