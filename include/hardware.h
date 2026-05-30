#pragma once
#include <Arduino.h>

namespace Hardware {
  // Call once in setup(). Configures pins, ADC resolution, primes gamepad state.
  void begin();

  // Call every loop(). Reads switches + pot; calls Gamepad.send() on change.
  void update();

  // Current physical switch positions (for sync comparison against DCS-BIOS readback)
  uint8_t  sw1Pos();  // AP PITCH: 0=ATT HOLD, 1=A/P OFF, 2=ALT HOLD
  uint8_t  sw2Pos();  // AP ROLL:  0=STRG SEL, 1=ATT HOLD, 2=HDG SEL
  uint16_t potRaw();  // ADC1 raw 0-4095
}
