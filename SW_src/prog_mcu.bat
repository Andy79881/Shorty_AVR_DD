@echo off
echo Erase and Program ...
set build_path=.pio\build\
set build_env=AVR32DD20_release
echo %build_path%%build_env%\firmware.hex
pymcuprog write -t uart -u COM4 -d avr32dd20 -f %build_path%%build_env%\firmware.hex --erase --verify
