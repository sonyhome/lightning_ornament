// Thunderstorm
//
// (c) 2024 Copyright Dan Truong Nov 2024
//
// The build consists in cotton balls shaped into a cloud, with tiny white LEDs
// embedded in them. The code will make LEDs blink randomly to simulate thunder
// within the clouds.
//
///////////////////////////////////////////////////////////////////////////////
// LED wiring:
//////////////
// The LED is connected to the pin with a resistor:
//
//  -----
// |     | Resistor  Diode
// | AVR |----[R]----|>|-----| Ground
// |     |
//  -----
//
// Vcc = R * id + Vd
//
// To calculate the resistor R, you need to know your power supply voltage,
// and the properties of the diode Vd and id at normal operating conditions
// (for example 30mA and 3.3V). If Vd is more than the battery, you can skip
// putting a resistor and the LED will be dim.
//
// If Vd is less, the resistor must take the slack, aka R = (Vcc-Vd)/id.
// Assuming some specs: R = (5V-3V)/30mA = 66 Ohm, so a value of 100 Ohms or
// more should be safe for most LEDs if you don't have the specifications.
//
// If the lightning strikes are very short (<4ms) then R could be ommited: the
// current surge will be brief and the LED won't have time to burn out. Papers
// indicate you can "overdrive" LEDs 10x or 50x just fine. They won't be 10x
// or 50x brighter but they won't be damaged. However the current going through
// might increase exponentially too, and that might strain the AtTiny85 output
// pin (spec safe limit is 40mA peak, 20mA sustained).
//
// Flashes don't need to last long for persistance of vision and look bright,
// however too short it will appear dimmer.
///////////////////////////////////////////////////////////////////////////////
// AtTiny85:
////////////
// The code can be modified but will target the AtTiny85 chip with 5 I/O pins,
// so as-is it can support up to 5 LEDs.
// To program an AtTiny85, use Arduino As ISP, use "upload using programmer",
// use 1MHz internal clock and burn the bootloader (low frequency is OK).
//             -----
// Reset PB5 -|     |- Vcc
//     3 PB3 -|     |- PB2 2
//     4 PB4 -|     |- PB1 1
//    Ground -|     |- PB0 0
//             -----
//
// The AtTiny I/O pin structure:
// In output mode, it will behave like a FET transistor delivering Vcc to the
// pin. It will also have shunt TVS clamping diodes connected to ground and Vcc
// to protect the pin from ESD discharges. These diodes will pass current if
// the pin's voltage is below the chip's Ground voltage or higher than Vcc.
// The spec is to enable clamping when the voltage is -0.5V or lower, via a
// 100Ohm resistor.
// 
// When the FET is open, it will pass through Vcc to the pin's load, but the
// specification current is 40mA or less.
//
// Max ratings
// Pin voltage: -0.5V ~ Vcc+0.5V (before clamping happens)
// Max Vcc: 6V
// Max pin current: 40mA
// Max Gnd, Vcc current: 200mA
///////////////////////////////////////////////////////////////////////////////
// First suggested circuit with 2 CR2032 batteries (max 6.4V)
//
// - put one diode on Gnd
// - wire LEDs directly to I/O pins 
//             -----
//           -|     |------- Vcc --- :|:| (6.4V)
//           -|     |-
//           -|     |--->|-- Gnd
// Gnd -->|---|     |-
//             -----
//
// Discussion: The diode will bring the voltage to the AtTiny85 down to 5.8V
// which is below the max limit, and may cause the clamping diodes to leak
// current through the LED. However the voltage would likely be 0.7-0.5 = 0.2V
// so the current going through the LED would be minuscule, way below the 3.3V
// where the LED would start using a lot of current.
//
// What is the maximum current on the I/O pin?
//
// LED forward current:
// White: 3.5V
//
// CR2032 battery internal resistance:
// Voltage   Ohm
// 2.7       400
// 2.8       20~50
// 2.9       15~20
// 3.0+       10~20
// Max steady draw: 15mA
//
// When the LED is on this gives the following equivalent diagram with new bat:
//
//    Vcc 6.4V ---[10Ohm]--->|--- 0V Gnd
//
// imax = (Vcc-Vd)/Rbat
// at 3.2V battery: (6.4-3.5)/10=290mA (max current)
// at 3.0V battery: (6-3.5)/20=125mA
// at 2.8V battery: (5.6-3.5)/50=32mA (LED is still full brightness)
// at 2.7V battery: (5.4-3.5)/400 = 5mA (LED is dim)
// 
///////////////////////////////////////////////////////////////////////////////
// Alternative:
//             -----
//           -|     |---|<-- Vcc --- :|:| (6.4V)
//           -|     |-     |
//           -|     |---|<--
//     Gnd ---|     |-
//             -----
//
// The diode is put on the Vcc pin, and the LED is reversed. When the IO pin is
// high, the LED is off, and when it is low, it grounds the LED and turns it on
// This solution makes the LED current drain to the ground via the chip,
// insteadof driving Vcc to the LED via the chip. It may be better because the
// chip can likely handle high currents better on the ground than on Vcc side.
///////////////////////////////////////////////////////////////////////////////
// Alternative:
//             -----
//           -|     |--------|<-- Vcc --- :|:| (6.4V)
//           -|     |-     |
//           -|     |---|<--
//     Gnd ---|     |-
//             -----
//
// The diode is put on the battery, so the LED and the Ic have the same Vcc.
// This avoids issues with the shunt LEDs, but lowers the voltage on the LED
// when the battery is nearly empty, however the loss seems negligible:
//
// at 2.8V battery: (5-3.5)/50=30mA (LED is still full brightness)
//
///////////////////////////////////////////////////////////////////////////////


// Uncomment to compile for AtTiny85, comment for Uno or others.
#define IS_ATTINY85_CONTROLLER

#ifdef IS_ATTINY85_CONTROLLER

#include "Arduino.h"
#include <avr/sleep.h>
#include <avr/power.h>
// https://electronics.stackexchange.com/questions/349496/how-to-achieve-low-power-consumption-with-attiny85
#define sleep_delay delay
#define DEBUG_INIT
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)

const uint8_t numLeds = 5;
// Pinout table. Use an indirection table to make code modular
uint8_t led_pin[numLeds] = {0, 1, 2, 3, 4}; // AtTiny85 PB0-5 (all pins)

#else

#define sleep_delay delay
#define DEBUG_INIT Serial.begin(9600)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)

const uint8_t numLeds = 5;
// Pinout table. Use an indirection table to make code modular
uint8_t led_pin[numLeds] = {3, 4, 5, 6, 7}; // Arduino Uno PD3-7 pins

#endif

// Define a thunder effect: First "long" lightning and "short" ripples
typedef struct delays {
  unsigned long longTimeStamp;   // Runtime value (ms)
  unsigned long shortTimeStamp;  // Runtime value (ms)
  uint8_t shortStrikeCount;      // Runtime value (ms)
  const unsigned long longMax;   // Max wait between strikes (ms)
  const unsigned long longMin;  // Maxwait between ripples (ms)
  const unsigned long shortMax;  // Maxwait between ripples (ms)
  const unsigned long shortMin;  // Maxwait between ripples (ms)
} delays_t;

// Specify the thunder timing values
delays_t thunder = { 0, 0, 0, 10000, 2000, 200, 1};

///////////////////////////////////////////////////////////////////////////////
void setup() {
  DEBUG_INIT;

  for(uint8_t i = 0; i < numLeds; i++)
  {
    pinMode(led_pin[i], OUTPUT);
  }

#ifdef IS_ATTINY85_CONTROLLER
  // Turn off modules to lower power consumption
  // disable the ADC (Analog/Digital conversion circuits)
  ADCSRA &= ~ bit(ADEN);
  power_adc_disable();
  // power_timer0_disable(); // Internal clock
  power_timer1_disable();
  power_usi_disable();

/* Hack to run at 20MHz, not needed here
  OSCCAL = 181;
  TCCR1 = 1<<CTC1 | 0<<COM1A0 | 1<<CS10; // CTC mode, /1
  GTCCR = 1<<COM1B0;                     // Toggle OC1B
  PLLCSR = 0<<PCKE;                      // System clock as clock source
  OCR1C = 0;
*/
#endif
}

///////////////////////////////////////////////////////////////////////////////
void loop() {
  // Wait time between main loop() iterations. We will try to sleep between
  // strikes to minimize power usage. Note: the standard delay() function does
  // not put the controller into sleep mode. We must replace the function with
  unsigned long loopDelay = 5;

  // Current time
  unsigned long millisecs = millis();
  
  // Time for main strike
  if (millisecs >= thunder.longTimeStamp) {
    lightning(chooseLed());
    // Set lightning ripples after current strike
    thunder.shortStrikeCount = random(2*numLeds);
    if (thunder.shortStrikeCount) {
      thunder.shortStrikeCount = numLeds;
    }
    thunder.shortTimeStamp = millisecs + thunder.shortMin + random(thunder.shortMax);

    // Set next main lightning strike
    thunder.longTimeStamp = millisecs + thunder.longMin + random(thunder.longMax);
    DEBUG_PRINT(millisecs);
    DEBUG_PRINT(" L:");
    DEBUG_PRINTLN(thunder.longTimeStamp);
  }

  // Time for a ripple strike
  if ((millisecs >= thunder.shortTimeStamp) && (thunder.shortStrikeCount > 0)) {
    lightningRipple(chooseLed());
    // Set next lightning in ripple sequence
    thunder.shortStrikeCount--;
    thunder.shortTimeStamp = millisecs + thunder.shortMin + random(thunder.shortMax);

    // Set a long wait for the next thunder to idle the CPU when doing nothing
    if (thunder.shortStrikeCount == 0) {
        loopDelay = thunder.longTimeStamp - millisecs;
    } else {
      loopDelay = thunder.shortTimeStamp - millisecs;
    }
  }

  // Make CPU sleep
  if (loopDelay) {
    if (loopDelay > 1000) DEBUG_PRINTLN(loopDelay);
    sleep_delay(loopDelay);
  }
}

///////////////////////////////////////////////////////////////////////////////
// @desc Randomly pick one of the LEDs and return its port number
uint8_t chooseLed(void)
{
  return led_pin[random(numLeds)];
}

///////////////////////////////////////////////////////////////////////////////
// @desc Turn on briefly an LED to simulate a big lightning strike
void lightning(uint8_t ledPin)
{
  // A nop takes one cycle or 65ns for an AtTiny86 at 16MHz
  const double us = 1000/65;

  digitalWrite(ledPin, HIGH);
  // Insert NOP instructions (more precise than delay())
  // https://gcc.gnu.org/onlinedocs/gcc/AVR-Built-in-Functions.html
  // __builtin_avr_nops(1000 * us);
  delay(1);
  digitalWrite(ledPin, LOW);
}

///////////////////////////////////////////////////////////////////////////////
// @desc Turn on very briefly an LED to simulate lightning ripples of thunder
void lightningRipple(uint8_t ledPin)
{
  // A nop takes one cycle or 65ns for an AtTiny86 at 16MHz
  const double us = 1000/65; // one microsecond

  digitalWrite(ledPin, HIGH);
  // Insert NOP instructions (more precise than delay())
  // https://gcc.gnu.org/onlinedocs/gcc/AVR-Built-in-Functions.html
  __builtin_avr_nops(100 * us);
  //delay(1); // 1ms = 1000us
  digitalWrite(ledPin, LOW);
}

