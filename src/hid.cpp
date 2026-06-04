#include "hid.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;
USBHIDGamepad  HID::Gamepad;

// --- Absolute mouse HID descriptor (64 bytes) ---
// X and Y declared separately so each can have its own LogMax (screen W ≠ H).
// Bytes 41-42 = X LogMax (lo, hi), bytes 54-55 = Y LogMax (lo, hi).
// Both are patched with (screenW-1) and (screenH-1) in HID::begin() before USB.begin().
// Report ID = HID_REPORT_ID_MOUSE (2). Payload = [buttons][Xlo][Xhi][Ylo][Yhi] = 5 bytes.
static uint8_t s_absBuf[64] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01,   // UsagePage(Desktop), Usage(Mouse), App Collection
  0x85, HID_REPORT_ID_MOUSE,          // Report ID (2)
  0x09,0x01, 0xA1,0x00,               // Usage(Pointer), Physical Collection
  0x05,0x09, 0x19,0x01, 0x29,0x03,   // UsagePage(Button), UsageMin(1), UsageMax(3)
  0x15,0x00, 0x25,0x01,               // LogMin(0), LogMax(1)
  0x95,0x03, 0x75,0x01, 0x81,0x02,   // Count(3), Size(1), Input — 3 button bits
  0x95,0x01, 0x75,0x05, 0x81,0x03,   // Count(1), Size(5), Input — 5 padding bits
  0x05,0x01,                          // UsagePage(Desktop)
  // X axis — LogMax lo/hi at bytes 41,42 — patched in HID::begin()
  0x09,0x30,                          // Usage(X)
  0x15,0x00, 0x26,0x7F,0x07,          // LogMin(0), LogMax(1919 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  // Y axis — LogMax lo/hi at bytes 54,55 — patched in HID::begin()
  0x09,0x31,                          // Usage(Y)
  0x15,0x00, 0x26,0x37,0x04,          // LogMin(0), LogMax(1079 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  0xC0,                               // End Physical Collection
  0xC0                                // End App Collection
};

static uint16_t s_maxX = 1919;
static uint16_t s_maxY = 1079;

class AbsMouseDevice : public USBHIDDevice {
public:
  AbsMouseDevice() {
    static bool s_init = false;
    if (!s_init) { s_init = true; _hid.addDevice(this, sizeof(s_absBuf)); }
  }
  void begin() { _hid.begin(); }
  bool ready() { return _hid.ready(); }
  uint16_t _onGetDescriptor(uint8_t* buf) override {
    memcpy(buf, s_absBuf, sizeof(s_absBuf));
    return sizeof(s_absBuf);
  }
  bool send(uint8_t btns, uint16_t x, uint16_t y) {
    uint8_t r[5] = {
      btns,
      (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
      (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    return _hid.SendReport(HID_REPORT_ID_MOUSE, r, 5);
  }
private:
  USBHID _hid;
};

static AbsMouseDevice s_absMouse;
static uint16_t       s_absX = 0;
static uint16_t       s_absY = 0;
static USBHID         s_hid;   // keep: removing shifts TinyUSB interface numbering

void HID::begin(uint16_t w, uint16_t h) {
  // Patch descriptor with screen-native coordinate ranges before USB enumeration
  s_maxX = w - 1;
  s_maxY = h - 1;
  s_absBuf[41] = s_maxX & 0xFF;
  s_absBuf[42] = s_maxX >> 8;
  s_absBuf[54] = s_maxY & 0xFF;
  s_absBuf[55] = s_maxY >> 8;

  USB.manufacturerName("E4 Mafia");
  USB.productName("AN/BRU-370");
  USB.PID(0x370A);
  Keyboard.begin();
  s_absMouse.begin();
  Gamepad.begin();
  USB.begin();
}

bool HID::isReady() {
  return s_absMouse.ready();
}

void HID::moveAbs(uint16_t x, uint16_t y) {
  s_absX = x;
  s_absY = y;
  s_absMouse.send(0, s_absX, s_absY);
}

void HID::moveRel(int16_t dx, int16_t dy) {
  int nx = (int)s_absX + dx;
  int ny = (int)s_absY + dy;
  moveAbs((uint16_t)constrain(nx, 0, (int)s_maxX),
          (uint16_t)constrain(ny, 0, (int)s_maxY));
}

void HID::pressKey(uint8_t key) {
  Keyboard.press(key); delay(50); Keyboard.release(key);
}

void HID::typeText(const char* text) {
  Keyboard.print(text); delay(250);
}

void HID::mouseClick(uint8_t button) {
  s_absMouse.send(button, s_absX, s_absY);
  delay(50);
  s_absMouse.send(0, s_absX, s_absY);
  delay(250);
  delay(20);
}
