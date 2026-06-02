#pragma once
#include <Arduino.h>

namespace Hardware {
  // Call once in setup(). Configures pins, ADC resolution, primes gamepad state.
  void begin();

  // Call every loop(). Reads switches; calls Gamepad.send() on change or every 2s.
  void update();

  // Force an immediate gamepad send on the next update() — call on DCS-BIOS connect.
  void forceSync();

  // Current physical switch positions (for sync comparison against DCS-BIOS readback)
  uint8_t sw1Pos();  // AP PITCH: 0=ATT HOLD, 1=A/P OFF, 2=ALT HOLD
  uint8_t sw2Pos();  // AP ROLL:  0=STRG SEL, 1=ATT HOLD, 2=HDG SEL
}
