#include "touch.h"
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

// Minimal GT911 polling driver. Registers (16-bit, big-endian addr):
//   0x814E status: bit7 = buffer ready, bits0-3 = touch count
//   0x8150 point 1: x_lo, x_hi, y_lo, y_hi  (already in panel pixels)
// After a read, 0 must be written back to 0x814E.
namespace {
  uint8_t s_addr = 0x5D;

  bool readRegs(uint16_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(s_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(s_addr, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
  }

  bool writeReg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(s_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  }

  void readCb(lv_indev_t*, lv_indev_data_t* data) {
    uint8_t status = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    if (!readRegs(0x814E, &status, 1)) return;
    if (!(status & 0x80)) return;              // no fresh buffer
    uint8_t n = status & 0x0F;
    if (n >= 1) {
      uint8_t p[4];
      if (readRegs(0x8150, p, 4)) {
        data->point.x = (int32_t)(p[0] | (p[1] << 8));
        data->point.y = (int32_t)(p[2] | (p[3] << 8));
        data->state = LV_INDEV_STATE_PRESSED;
      }
    }
    writeReg(0x814E, 0);
  }
}

namespace Touch {

bool begin() {
  uint8_t id[4] = {0};
  s_addr = 0x5D;
  if (!readRegs(0x8140, id, 4)) {              // product ID "911"
    s_addr = 0x14;
    if (!readRegs(0x8140, id, 4)) return false;
  }
  Serial.printf("GT911 at 0x%02X, ID %c%c%c\n", s_addr, id[0], id[1], id[2]);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readCb);
  return true;
}

}  // namespace Touch
