#include "hid.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;
USBHIDGamepad  HID::Gamepad;

// --- Absolute mouse HID descriptor (51 bytes) ---
// Pointer device, 0-32767 logical range, 3 buttons + X/Y axes.
// Report: [buttons(1B)] [X lo][X hi] [Y lo][Y hi]  = 5 bytes total.
static const uint8_t s_absDesc[] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01,   // UsagePage(Desktop), Usage(Mouse), App Collection
    0x09,0x01, 0xA1,0x00,             // Usage(Pointer), Physical Collection
      0x05,0x09,                      // UsagePage(Button)
      0x19,0x01, 0x29,0x03,           // UsageMin(1) UsageMax(3)
      0x15,0x00, 0x25,0x01,           // LogMin(0) LogMax(1)
      0x95,0x03, 0x75,0x01,           // Count(3) Size(1)
      0x81,0x02,                      // Input(Data,Var,Abs) — 3 button bits
      0x95,0x01, 0x75,0x05,           // Count(1) Size(5)
      0x81,0x03,                      // Input(Const) — 5 padding bits
      0x05,0x01,                      // UsagePage(Desktop)
      0x09,0x30, 0x09,0x31,           // Usage(X) Usage(Y)
      0x15,0x00, 0x26,0xFF,0x7F,      // LogMin(0) LogMax(32767)
      0x75,0x10, 0x95,0x02,           // Size(16) Count(2)
      0x81,0x02,                      // Input(Data,Var,Abs) — X and Y
    0xC0,                             // End Physical Collection
  0xC0                                // End App Collection
};

class AbsMouseDevice : public USBHIDDevice {
public:
  AbsMouseDevice() {
    static bool s_init = false;
    if (!s_init) { s_init = true; _hid.addDevice(this, sizeof(s_absDesc)); }
  }
  void begin() { _hid.begin(); }
  uint16_t _onGetDescriptor(uint8_t* buf) override {
    memcpy(buf, s_absDesc, sizeof(s_absDesc));
    return sizeof(s_absDesc);
  }
  bool send(uint8_t btns, uint16_t x, uint16_t y) {
    uint8_t r[5] = {
      btns,
      (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
      (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    return _hid.SendReport(0, r, 5);
  }
private:
  USBHID _hid;
};

static AbsMouseDevice s_absMouse;
static uint16_t       s_absX = 0;
static uint16_t       s_absY = 0;
static USBHID         s_hid;   // used only for isReady() query

void HID::begin() {
  USB.manufacturerName("E4 Mafia");
  USB.productName("AN/BRU-370");
  USB.PID(0x370A);
  Keyboard.begin();
  s_absMouse.begin();
  Gamepad.begin();
  USB.begin();
}

bool HID::isReady() {
  return s_hid.ready();
}

void HID::moveAbs(uint16_t x, uint16_t y) {
  s_absX = x;
  s_absY = y;
  s_absMouse.send(0, s_absX, s_absY);
}

void HID::moveRel(int16_t dx, int16_t dy) {
  int nx = (int)s_absX + dx;
  int ny = (int)s_absY + dy;
  moveAbs((uint16_t)constrain(nx, 0, 32767),
          (uint16_t)constrain(ny, 0, 32767));
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
}
