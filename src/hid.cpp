#include "hid.h"
#include "config.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;

// --- Absolute digitizer (pen) HID descriptor (64 bytes) ---
// UsagePage(Digitizer)/Usage(Pen) bypasses Windows mouhid.sys delta pipeline entirely.
// Windows hidpen.sys maps absolute coordinates directly — no threshold, no EPP, no ballistics.
// Report payload unchanged: [btns][Xlo][Xhi][Ylo][Yhi] = 5 bytes, Report ID = 2.
// btns byte: bit0=TipSwitch(L-click), bit1=BarrelSwitch(R-click), bit2=InRange(must=1), bits3-7=0.
// Bytes 41-42 = X LogMax (lo, hi), bytes 54-55 = Y LogMax (lo, hi) — same patch offsets as before.
static uint8_t s_absBuf[64] = {
  0x05,0x0D, 0x09,0x02, 0xA1,0x01,   // UsagePage(Digitizer), Usage(Pen), App Collection
  0x85, HID_REPORT_ID_MOUSE,          // Report ID (2)
  0x09,0x20, 0xA1,0x00,               // Usage(Stylus), Logical Collection
  0x09,0x42, 0x09,0x44, 0x09,0x32,   // Usage(TipSwitch), Usage(BarrelSwitch), Usage(InRange)
  0x15,0x00, 0x25,0x01,               // LogMin(0), LogMax(1)
  0x75,0x01, 0x95,0x03, 0x81,0x02,   // Size(1), Count(3), Input(Abs) — 3 control bits
  0x75,0x05, 0x95,0x01, 0x81,0x03,   // Size(5), Count(1), Input(Const) — 5 padding bits
  0x05,0x01,                          // UsagePage(Desktop)
  // X axis — LogMax lo/hi at bytes 41,42 — patched in HID::begin()
  0x09,0x30,                          // Usage(X)
  0x15,0x00, 0x26,0x7F,0x13,          // LogMin(0), LogMax(5119 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  // Y axis — LogMax lo/hi at bytes 54,55 — patched in HID::begin()
  0x09,0x31,                          // Usage(Y)
  0x15,0x00, 0x26,0x9F,0x05,          // LogMin(0), LogMax(1439 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  0xC0,                               // End Logical Collection
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
      (uint8_t)(btns | 0x04),   // bit2 = InRange; pen must report in-range to track cursor
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
  USB.firmwareVersion(FIRMWARE_VERSION_INT);
  Keyboard.begin();
  s_absMouse.begin();
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
  delay(50);
}
