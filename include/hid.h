#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDGamepad.h>

namespace HID {
  extern USBHIDKeyboard Keyboard;
  extern USBHIDMouse    Mouse;
  extern USBHIDGamepad  Gamepad;

  // Call once in setup() — begins all three HID devices + USB stack
  void begin();

  // True if USB host has enumerated the device
  bool isReady();

  // Helpers matching Sham_Master_100 BleCombo API (used by macros.cpp)
  void pressKey(uint8_t key);
  void typeText(const char* text);
  void mouseClick(uint8_t button = MOUSE_LEFT);
  void homeMouse();
  void moveMouseTotal(int dx, int dy);
}
