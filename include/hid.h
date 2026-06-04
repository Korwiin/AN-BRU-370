#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDGamepad.h>

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 0x01
#endif

namespace HID {
  extern USBHIDKeyboard Keyboard;
  extern USBHIDGamepad  Gamepad;

  void begin(uint16_t w, uint16_t h);
  bool isReady();

  void moveAbs(uint16_t x, uint16_t y);
  void moveRel(int16_t dx, int16_t dy);

  void pressKey(uint8_t key);
  void typeText(const char* text);
  void mouseClick(uint8_t button = MOUSE_LEFT);
}
