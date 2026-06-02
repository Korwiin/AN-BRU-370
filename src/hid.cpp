#include "hid.h"

USBHIDKeyboard HID::Keyboard;
USBHIDMouse    HID::Mouse;
USBHIDGamepad  HID::Gamepad;

static USBHID s_hid;  // used only for ready() query

void HID::begin() {
  USB.manufacturerName("E4 Mafia");
  USB.productName("AN/BRU-370");
  USB.PID(0x370A);
  Keyboard.begin();
  Mouse.begin();
  Gamepad.begin();
  USB.begin();
}

bool HID::isReady() {
  return s_hid.ready();
}

void HID::pressKey(uint8_t key) {
  Keyboard.press(key); delay(50); Keyboard.release(key);
}

void HID::typeText(const char* text) {
  Keyboard.print(text); delay(250);
}

void HID::mouseClick(uint8_t button) {
  Mouse.click(button); delay(250);
}

void HID::homeMouse() {
  for (int i = 0; i < 53; i++) {
    Mouse.move(-127, -30);
    delay(20);
  }
  delay(250);
}

void HID::moveMouseTotal(int totalX, int totalY) {
  int iters = max((abs(totalX) + 126) / 127, (abs(totalY) + 126) / 127);
  if (iters == 0) { delay(250); return; }
  int sentX = 0, sentY = 0;
  for (int i = 1; i <= iters; i++) {
    int nx = (totalX * i) / iters;
    int ny = (totalY * i) / iters;
    Mouse.move((int8_t)(nx - sentX), (int8_t)(ny - sentY));
    sentX = nx; sentY = ny;
    delay(20);
  }
  delay(250);
}
