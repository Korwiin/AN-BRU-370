#pragma once
#include <Arduino.h>

#ifndef DEV_BUILD
// Production build: full HID (keyboard + absolute digitizer)
#include <USB.h>
#include <USBHIDKeyboard.h>

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 0x01
#endif

namespace HID {
  extern USBHIDKeyboard Keyboard;

  void begin(uint16_t w, uint16_t h);
  bool isReady();

  void moveAbs(uint16_t x, uint16_t y);
  void moveRel(int16_t dx, int16_t dy);

  void pressKey(uint8_t key);
  void typeText(const char* text);
  void mouseClick(uint8_t button = MOUSE_LEFT);
}

#else
// Dev build: HID entirely stubbed — USB.begin() must NOT be called.
// CDC Serial owns the USB-OTG port; calling USB.begin() (which TinyUSB
// requires for HID) conflicts with it and breaks Serial (v0.84 incident).
#ifndef MOUSE_LEFT
#define MOUSE_LEFT 0x01
#endif

// Key constants normally provided by USBHIDKeyboard.h — replicated here
// so call sites in main.cpp and macros.cpp compile without changes.
#ifndef KEY_LEFT_CTRL
#define KEY_LEFT_CTRL 0x80
#endif
#ifndef KEY_F1
#define KEY_F1  0xC2
#endif
#ifndef KEY_F10
#define KEY_F10 0xCB
#endif

namespace HID {
  struct NullKeyboard {
    void releaseAll() {}
    void press(uint8_t) {}
  };
  extern NullKeyboard Keyboard;

  inline void begin(uint16_t, uint16_t) {}
  inline bool isReady()                 { return false; }
  inline void moveAbs(uint16_t, uint16_t) {}
  inline void moveRel(int16_t, int16_t)   {}
  inline void pressKey(uint8_t)           {}
  inline void typeText(const char*)       {}
  inline void mouseClick(uint8_t = MOUSE_LEFT) {}
}
#endif
