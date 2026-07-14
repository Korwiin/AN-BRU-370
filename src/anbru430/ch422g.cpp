#include "ch422g.h"
#include <Wire.h>

namespace {
  constexpr uint8_t ADDR_WR_SET = 0x24;
  constexpr uint8_t ADDR_WR_IO  = 0x38;

  bool writeByte(uint8_t addr, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  }
}

namespace CH422G {
  bool begin() { return writeByte(ADDR_WR_SET, 0x01); }
  bool setOutputs(uint8_t mask) { return writeByte(ADDR_WR_IO, mask); }
}
