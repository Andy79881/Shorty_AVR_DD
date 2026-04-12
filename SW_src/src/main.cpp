/*
 * shorty.cpp
 *
 * V1.0
 * Created on: Oct 21, 2025
 * Author: Andreas Nestler based on source code from kripton2035
 * forked from http://kripton2035.free.fr/Continuity%20Meters/shorty-with-disa.html 
 * changes
 * - Target MCU AVR32DD20 
 * - internal ADC handling modified by help of Atmel Start
 * - internal ADC: VREF internal 2.048V, resolution: 12 bit, accumulate 8 samples
 * - #ifdef NO_VBAT_SCALE added => flag depending battery voltage scaling (build-flag: -D NO_VBAT_SCALE)
 * - convertandread funct. hangs -> replaced by dedicated convert & read functions with delay inbetween
 * - delay > 60ms required -> set to 65ms
 * - for proper delay values adjust F_CPU according clock init (20MHz)
 * V1.1: Update 01.11.2025
 * - flag based convert and read of MCP3421 to periodically read the device and check the config result
 * - no blocking delay of ~65ms required
 * - CPU clock: 16Mhz to save power
 * - battery gauge : full battery value change 3V -> 2.8V for 2x NiMH
 * V1.2: Update 02.11.2025
 * - Auto Power-off after Timeout with millis()
 * - usage of tone() requires change of timer for millis() -> build-flag: -D MILLIS_USE_TIMERB1
 * - Enable with button press and release
 * V1.3: Update 08.11.2025
 * - RTC with 1Hz interrupt used instead of millis() for auto power-off
 * - build-flag: -D MILLIS_USE_TIMERB1 NOT needed
 * V1.4: Update 06.12.2025 
 * - battery measurement less frequently (e.g., once every second) since Vbat changes very slowly
 * - usage of tone() requires change of timer for millis() -> build-flag: -D MILLIS_USE_TIMERB1
 */

// #include <Arduino.h> // added to comply with cpp instead of ino
// #include <avr/io.h>
#include <util/delay.h>
#include <EEPROM.h>
#include <Wire.h>
#include <MCP342x.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/sleep.h>
#include <avr/power.h>

// ---------------------------------------------------------------------------------------------
//    some macros definitions
// --------------------------------------------------------------------------------------------- 

// F_CPU defined in make file or platformio.ini
#ifndef F_CPU
#define F_CPU 24000000UL
#endif

// ---------------------------------------------------------------------------------------------
//    pins definition
// --------------------------------------------------------------------------------------------- 
#define  BUZZER       PIN_PC3
#define  BUTTON       PIN_PC2
#define  DBG          PIN_PC1
#define  PWR_SW       PIN_PA6
#define  AOP          PIN_PD5

// ---------------------------------------------------------------------------------------------
//    Useful macros
// --------------------------------------------------------------------------------------------- 
#define PC2_INTERRUPT PORTC.INTFLAGS & PIN2_bm // interrupt PC2?
#define PC2_CLEAR_INTERRUPT_FLAG PORTC.INTFLAGS &= PIN2_bm // clear interrupt by writing a '1'

// ---------------------------------------------------------------------------------------------
//    display constants for a 128x32 oled module
// --------------------------------------------------------------------------------------------- 
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/** Datatype for the result of the ADC conversion */
typedef uint16_t adc_result_t;
//* Analog channel selection */
typedef ADC_MUXPOS_t adc_0_channel_t;

// ---------------------------------------------------------------------------------------------
//    analog to digital constants
// --------------------------------------------------------------------------------------------- 
// 0x68 is the default address for all MCP342x devices
uint8_t address = 0x68;
MCP342x adc = MCP342x(address);
// added code snippet below
// Configuration settings
MCP342x::Config config(MCP342x::channel1, MCP342x::oneShot,
  MCP342x::resolution16, MCP342x::gain1);
// Configuration/status read back from the ADC
MCP342x::Config status;


unsigned long probes_compensation;   // 80mohms de sonde => 1V sortie aop, 5mV par count
unsigned long adc_val;   // 0-65535+

unsigned int button_delay;  // to detect long press of the button
float Vbat;
float Rx;
// Inidicate if a new conversion should be started
bool startConversion = false;
unsigned long lastBatteryMeasure = 0;

// constants for usage of internal ADC
#define RES_TOP 22.0 // voltage divider top resistor (kohms)
#define RES_BOT 22.0 // voltage divider bottom resistor (kohms)
#define VREF_ADC 2.048f; // ADC reference voltage
#ifdef NO_VBAT_SCALE // Use -D Flag in platformio.ini to disable resistive voltage divider
const float SCALE_VBAT = VREF_ADC;
#else
const float SCALE_VBAT = (RES_TOP+RES_BOT)/RES_BOT*VREF_ADC;
#endif
const uint8_t SAMPNUM_ACC_ADC = ADC_SAMPNUM_ACC8_gc; // sample accumulation number
#define ADC_SHIFT_DIVx SAMPNUM_ACC_ADC // to get average value if accumulator used in ADC init

// Constants for RTC
const uint32_t SLEEP_AFTER = (uint32_t)5*60;  // 5min in seconds

// put function declarations here: added to comply with cpp instead of ino
void SetupRTC(void);
void BatteryIcon(float);
void DisplayOmega( );
void DisplayStr(char *);
void io_cfg_init(void);
void CLKCTRL_init();
void ADCSetup();
void sleepDisplay(Adafruit_SSD1306*);
void wakeDisplay(Adafruit_SSD1306*);
adc_result_t ADC_GetConversion(adc_0_channel_t);


// Real-Time Clock **********************************************

volatile uint32_t Seconds;

void SetupRTC () {
  // Internal 32.768kHz Oscillator
  RTC.CLKSEL = RTC_CLKSEL_OSC32K_gc;
  
  // RTC Clock Cycles 32768, enabled ie 1Hz interrupt
  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm; 
  RTC.PITINTCTRL = RTC_PI_bm;                         // Periodic Interrupt enabled
}

// Interrupt Service Routine called every second
ISR(RTC_PIT_vect) {
  RTC.PITINTFLAGS = RTC_PI_bm;                        // Clear interrupt flag
  Seconds--;
}

// Pin Change interrupt service routine - wakes up
ISR (PORTC_PORT_vect) {
 
  if (PC2_INTERRUPT)
  {
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_INTDISABLE_gc;
    Seconds = SLEEP_AFTER;        // Reset timeout counter - check if needed
    RTC.PITINTCTRL = RTC_PI_bm;  // Periodic Interrupt enabled
    wakeDisplay(&display);
    digitalWriteFast(PWR_SW, LOW);  //  Switch-on power for resistive divider
    PC2_CLEAR_INTERRUPT_FLAG;       // Need to clear interrupt flag
  }
 
}

void io_cfg_init(void)
{
    // Port setup: IO-config
    PORTA.DIRSET = PIN7_bm | PIN6_bm;  // PA7, 6 output
    PORTC.DIRSET = 0b00001010;         // PC1, PC3 output
    PORTD.DIRSET = PIN6_bm;            // PD6 output    
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm; // PC2 enable pull-up
    // Use -D Flag in platformio.ini to enable uC internal Pull-Ups for I2C
    #ifdef INT_I2C_PULLUPEN 
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm; // PA2 enable pull-up
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm; // PA3 enable pull-up
    #endif

    // PD4, PD5, PD6: Disable digital input buffer, disable pull-up resistor
    // needed for ADC input(s) and DAC output(s)
    PORTD.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTD.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;

}

void CLKCTRL_init()
{
  // enable clock out at PA7 for debugging only to verify proper clock setup
	_PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_FREQSEL_16M_gc); /* set HF OSC freq to 16MHz */
  _PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_OSCHF_gc /* select internal high-frequency oscillator */
                 | 0 << CLKCTRL_CLKOUT_bp /* System clock out: 0/ 1: disabled/ enabled -> PA7 */);

}

void ADCSetup() {
  VREF.ADC0REF = VREF_REFSEL_2V048_gc;                 // reference voltage
  ADC0.CTRLB = SAMPNUM_ACC_ADC;                        // n results accumulated
	ADC0.CTRLC = ADC_PRESC_DIV32_gc;                     // CLK_PER divided by 32
  ADC0.MUXPOS = ADC_MUXPOS_VDDDIV10_gc;                // VDD/10
  ADC0.CTRLA = ADC_ENABLE_bm;                          // Single, 12-bit, right adjust
}


adc_result_t ADC_GetConversion(adc_0_channel_t channel)
{
	adc_result_t res;

  // digitalWriteFast(DBG, HIGH); // ON time of DBG pin determines the ADC conversion time

	ADC0.CTRLA &= ~ADC_CONVMODE_bm; // conversion mode: single ended -> may be omitted
  // bit clear only needed, if you switch between conversion modes (single ended/ differential)
  ADC0.MUXPOS  = channel;
	ADC0.COMMAND = ADC_STCONV_bm; // start single conversion
	while (!(ADC0.INTFLAGS & ADC_RESRDY_bm)); // conversion finished?
	res = ADC0.RES;  
	ADC0.INTFLAGS |= ADC_RESRDY_bm; // clear RESRDY flag
  // digitalWriteFast(DBG, LOW); 
	return res;
}


void setup( void)
{
  io_cfg_init();
  CLKCTRL_init();  // internal, 16MHz
  SetupRTC ();
  ADCSetup();
  // First time loop() is called start a conversion
  startConversion = true;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // Start running
  Seconds = SLEEP_AFTER;
 
// ---------------------------------------------------------------------------------------------
//    reads the stored probe compensation from the arduino eeprom
// --------------------------------------------------------------------------------------------- 
  probes_compensation = EEPROM.read( 0); // read probes compensation value from eeprom at startup
  probes_compensation += EEPROM.read( 1) << 8;
  probes_compensation += EEPROM.read( 2) << 16;
  probes_compensation += EEPROM.read( 3) << 24;
  //Serial.print("Probes:");Serial.println(probes_compensation);
  
 // noTone( BUZZER);
  
// Due to pin access limitations swap method may be used for small packages of DD devices
// for 20 pin package PA2 (SDA) and PA3 (SCL) available - no swap needed
//  Wire.pins(PIN_PA2, PIN_PA3);                   // omitted since default
  Wire.begin();                                  // join I2C bus

  
// ---------------------------------------------------------------------------------------------
//    initialize mcp3421 ADC
// --------------------------------------------------------------------------------------------- 
  MCP342x::generalCallReset();
  _delay_ms(1); // MC342x needs 300us to settle, wait 1ms

  
// ---------------------------------------------------------------------------------------------
//    initialize OLED display
// --------------------------------------------------------------------------------------------- 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c, 0);  // initialize display @ address 0x3c, no reset
  display.clearDisplay();
  display.display();

// ---------------------------------------------------------------------------------------------
//    powers P-FET transistor
// --------------------------------------------------------------------------------------------- 
  //  clrPinAnalog( TRANSISTOR);
  digitalWriteFast(PWR_SW, LOW); //  Arduino function for better readability
  _delay_ms(1);
}

void loop (void)
{
  uint8_t err;
 
  // ---------------------------------------------------------------------------------------------
  //    measures battery voltage 
  // --------------------------------------------------------------------------------------------- 
    if (millis() - lastBatteryMeasure > 1000) { // measure once a second avoiding flicker
      lastBatteryMeasure = millis();
      // int32_t adc_reading = analogReadEnh(PIN_PD4, 13);  
      adc_result_t adc_reading = ADC_GetConversion(ADC_MUXPOS_AIN4_gc) >> ADC_SHIFT_DIVx;
      Vbat = adc_reading*SCALE_VBAT/ 4096.0; // 1:1 divider (4.096V FS with 2.048V int reference)
    }

  // ---------------------------------------------------------------------------------------------
  //    read ADC mcp3421 from i2c bus
  // --------------------------------------------------------------------------------------------- 
      long value = 0;

  //    MCP342x::Config status;
    // Initiate a conversion; convertAndRead() will wait until it can be read
    // uint8_t err = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
    //        MCP342x::resolution16, MCP342x::gain1, 1000000, value, status);
    digitalWriteFast(DBG, LOW);
    if (startConversion) {
      err = adc.convert(config); 
      startConversion = false;
    }

    err = adc.read(value, status);
    if (!err && status.isReady()) { 
      adc_val = value;
      startConversion = true;
      digitalWriteFast(DBG, HIGH);
    }
  
  // ---------------------------------------------------------------------------------------------
  //    reads button state to engage REL mode
  // --------------------------------------------------------------------------------------------- 
      if (digitalReadFast( BUTTON) == 0) // check D3 button state
      {
        noTone( BUZZER);
        display.clearDisplay();
        DisplayStr("REL");
        display.display();
        _delay_ms(500);
        probes_compensation = adc_val;
        button_delay = 0;
        while ((digitalRead( BUTTON) == 0) && (button_delay<10))
        {
          button_delay++;
          _delay_ms(100);
        }
  // ---------------------------------------------------------------------------------------------
  //    stores probe compensation in eeprom if long press button
  // --------------------------------------------------------------------------------------------- 
        if (button_delay>8)
        {
          display.clearDisplay();
          DisplayStr("STORE");
          display.display();
          _delay_ms(500);
          EEPROM.update( 0, (byte) adc_val);  // stores probes compensation value in eeprom if different
          EEPROM.update( 1, (byte) adc_val >> 8);
          EEPROM.update( 2, (byte) adc_val >> 16);
          EEPROM.update( 3, (byte) adc_val >> 24);
          noTone( BUZZER);
          tone( BUZZER, 2000, 200);    // warn users of the eeprom update
          noTone( BUZZER);
          tone( BUZZER, 1000, 200);
          noTone( BUZZER);
          display.clearDisplay();
          DisplayStr("OK");
          display.display();
          _delay_ms(500);
        }
      }
  // ---------------------------------------------------------------------------------------------
  //    substract probe compensation from adc read value
  // --------------------------------------------------------------------------------------------- 
      long real_adc = adc_val - probes_compensation;
      if (real_adc<=0) real_adc = 0;
      
      float volts = 2.048 * ((float)(real_adc)/32768);    // converts adc reading to volts
  // ---------------------------------------------------------------------------------------------
  //    converts volts reading to resistance using inverse R approximation
  // --------------------------------------------------------------------------------------------- 
  // Opamp-Gain = 27, R1=100R, R2=2R, R3=1R, VCC=3.3
      Rx = (202*volts)/(178.2f-103*volts); // adapt to HW and supply voltage accordingly  
     
  // ---------------------------------------------------------------------------------------------
  //    final adjustment after reading the real value from the probes
  // --------------------------------------------------------------------------------------------- 
      float lcr_measured_res = 1.005;   // calibrated using a 1 ohm resistor
      float shorty_measured_res = 0.853;
      float calibration_factor = lcr_measured_res/shorty_measured_res;
      Rx = Rx * calibration_factor;          

  // ---------------------------------------------------------------------------------------------
  //    converts resistance to tone frequency 0 (open probes) to 4KHz (shorted probes)
  // --------------------------------------------------------------------------------------------- 
      if (adc_val>32500) {
        Rx = 9999;
        noTone( BUZZER);
      } else {
        long adc2 = adc_val/32; // maps 16 bits into 10
        // maps adc value to tone frequency from 0 to 4KHz exponential way for better difference in low ohms
        float ftone = -386.2467 + (4000 - -386.2467)/(1 + pow(adc2/26.40582,0.6354983));
        tone( BUZZER, ftone);
      }
      
  // ---------------------------------------------------------------------------------------------
  //    display resistance value on oled
  // --------------------------------------------------------------------------------------------- 
      display.clearDisplay();                // ... and display result
      BatteryIcon((Vbat-2.0)/(2.8-2.0));     // 2.0V = 0%, 2.8V = 100%
      display.setTextSize(3);
      display.setTextColor(WHITE);
      display.setCursor(0,10);
      if (Rx>1000) {
        display.print("-----");
        noTone( BUZZER);
      } else {
        display.print(Rx,4);      
      }
      DisplayOmega();
      display.display();

 

  if (Seconds == 0)  // Go to sleep?
  {
    // Turn off RTC interrupt otherwise PIT is continued to run even in all sleep modes
    RTC.PITINTCTRL = 0;
    // Power saving
    digitalWriteFast(PWR_SW, HIGH); //  Switch-off power for resistive divider
    display.clearDisplay();
    DisplayStr("SLEEP");
    display.display();
    _delay_ms(1000);
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;   // Enable Pin Change interrupt
    sleepDisplay(&display);
    _delay_ms(100);
    sleep_enable();
    sleep_cpu();                                              // Go to sleep
    Seconds = SLEEP_AFTER;                                     // Reset timeout counter
 
  } 
   
} // endless loop

void BatteryIcon(float charge)
// ---------------------------------------------------------------------------------------------
//   Draw a battery charge icon into the display buffer without refreshing the display
//     - charge ranges from 0.0 (empty) to 1.0 (full)
// ---------------------------------------------------------------------------------------------
  {
    int w = constrain(charge, 0.0, 1.0)*16;  // 0 to 16 pixels wide depending on charge
    if (charge<0) {
      w = 0;
    }
    display.drawRect(100, 0, 16, 7, WHITE);  // outline
    display.drawRect(116, 2,  3, 3, WHITE);  // nib
    display.fillRect(100, 0,  w, 7, WHITE);  // charge indication
  }

void DisplayOmega( void)
{
    static const unsigned char omega_bmp[] =     // omega (ohm) symbol
    { B00000011, B11000000,
      B00001111, B11110000,
      B00111000, B00011100,
      B01100000, B00000110,
      B01100000, B00000110,
      B11000000, B00000011,
      B11000000, B00000011,
      B11000000, B00000011,
      B11000000, B00000011,
      B11000000, B00000011,
      B01100000, B00000110,
      B01100000, B00000110,
      B01100000, B00000110,
      B00110000, B00001100,
      B11111000, B00011111,
      B11111100, B00111111 };
      
    // display omega (ohms) symbol
    display.drawBitmap(110, 14, omega_bmp, 16, 16, WHITE);

}

void DisplayStr(char *s)
// ---------------------------------------------------------------------------------------------
//    Adds a string, s, to the display buffer without refreshing the display @ (0,20)
// --------------------------------------------------------------------------------------------- 
  {
    display.setTextSize(3);              
    display.setTextColor(WHITE);
    display.setCursor(0,10);
    display.print(s);
  }
	

  void sleepDisplay(Adafruit_SSD1306* display) {
    display->ssd1306_command(SSD1306_DISPLAYOFF);
  }
  
  void wakeDisplay(Adafruit_SSD1306* display) {
    display->ssd1306_command(SSD1306_DISPLAYON);
  }