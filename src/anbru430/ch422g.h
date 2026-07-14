#pragma once
#include <Arduino.h>

// Minimal CH422G IO expander driver. The chip has no register map — it
// responds at fixed I2C addresses per function (datasheet §7):
//   0x24 WR_SET (system: 0x01 = IO outputs push-pull enabled)
//   0x38 WR_IO  (write IO0..IO7 output levels)
//   0x26 RD_IO  (read IO0..IO7)
namespace CH422G {
  bool begin();                     // Wire must already be initialised
  bool setOutputs(uint8_t mask);    // bit n drives EXIOn
}
