#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDGamepad.h>
#include "pins.h"
#include "config.h"

USBHIDKeyboard  Keyboard;
USBHIDMouse     Mouse;
USBHIDGamepad   Gamepad;

void setup() {
  Keyboard.begin();
  Mouse.begin();
  Gamepad.begin();
  USB.begin();

  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("=== Brew370 USB test ===");
  Serial.println("CDC serial: OK");
  Serial.println("HID keyboard + mouse + gamepad: initialised");
  Serial.println("Bridge GPIO10 to GND to test all three");
}

void loop() {
  if (digitalRead(PIN_ENC_SW) == LOW) {
    delay(50);
    if (digitalRead(PIN_ENC_SW) == LOW) {
      // Keyboard
      Keyboard.press('a'); delay(50); Keyboard.release('a');
      // Mouse
      Mouse.click(MOUSE_LEFT);
      // Gamepad -- button 0 press + x axis mid-point, then release
      // API: send(x, y, z, rz, rx, ry, hat, buttons)
      Gamepad.send(64, 0, 0, 0, 0, 0, HAT_CENTER, 1);
      delay(200);
      Gamepad.send(0, 0, 0, 0, 0, 0, HAT_CENTER, 0);
      Serial.println("HID: keyboard 'a' + mouse click + gamepad btn0/axis sent");
      while (digitalRead(PIN_ENC_SW) == LOW) {}
    }
  }
}
