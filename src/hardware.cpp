#include "hardware.h"
#include "pins.h"
#include <USB.h>
#include <USBHIDGamepad.h>

// Gamepad is defined here for Task 5. When Task 7 creates hid.cpp,
// this definition moves there and hardware.cpp will use extern + include hid.h.
USBHIDGamepad Gamepad;

static uint8_t  s_sw1    = 0xFF;
static uint8_t  s_sw2    = 0xFF;
static uint16_t s_potRaw = 0;

static uint8_t readSwitch(uint8_t pinA, uint8_t pinB) {
  bool a = (digitalRead(pinA) == LOW);
  bool b = (digitalRead(pinB) == LOW);
  if  (a && !b) return 0;
  if (!a && !b) return 1;  // center — no throw connected
  if (!a &&  b) return 2;
  return 1;                // both LOW = wiring fault, default center
}

static uint16_t readPot() {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(PIN_POT);
  return (uint16_t)(sum / 4);  // 0-4095
}

// Gamepad button mapping:
//   Bit 0 = SW1 pos 0 (AP PITCH ATT HOLD — held while switch is up)
//   Bit 1 = SW1 pos 2 (AP PITCH ALT HOLD — held while switch is down)
//   Bit 2 = SW2 pos 0 (AP ROLL  STRG SEL — held while switch is up)
//   Bit 3 = SW2 pos 2 (AP ROLL  HDG SEL  — held while switch is down)
//   Center (pos 1) = no bit set for that switch
// x-axis: pot ADC 0-4095 mapped to int8_t -127..127
// API: Gamepad.send(x, y, z, rz, rx, ry, hat, buttons)
static void pushGamepad(uint8_t sw1, uint8_t sw2, uint16_t potRaw) {
  uint32_t btns = 0;
  if (sw1 == 0) btns |= (1u << 0);
  if (sw1 == 2) btns |= (1u << 1);
  if (sw2 == 0) btns |= (1u << 2);
  if (sw2 == 2) btns |= (1u << 3);

  int8_t axis = (int8_t)(((int32_t)potRaw * 254 / 4095) - 127);

  Gamepad.send(axis, 0, 0, 0, 0, 0, HAT_CENTER, btns);
}

void Hardware::begin() {
  pinMode(PIN_SW1_A, INPUT_PULLUP);
  pinMode(PIN_SW1_B, INPUT_PULLUP);
  pinMode(PIN_SW2_A, INPUT_PULLUP);
  pinMode(PIN_SW2_B, INPUT_PULLUP);
  analogReadResolution(12);

  s_sw1    = readSwitch(PIN_SW1_A, PIN_SW1_B);
  s_sw2    = readSwitch(PIN_SW2_A, PIN_SW2_B);
  s_potRaw = readPot();
  pushGamepad(s_sw1, s_sw2, s_potRaw);
}

void Hardware::update() {
  uint8_t  sw1 = readSwitch(PIN_SW1_A, PIN_SW1_B);
  uint8_t  sw2 = readSwitch(PIN_SW2_A, PIN_SW2_B);
  uint16_t pot = readPot();

  bool swChanged  = (sw1 != s_sw1) || (sw2 != s_sw2);
  bool potChanged = abs((int)pot - (int)s_potRaw) > 32;  // ~0.8% hysteresis

  if (swChanged || potChanged) {
    s_sw1    = sw1;
    s_sw2    = sw2;
    s_potRaw = pot;
    pushGamepad(sw1, sw2, pot);
  }
}

uint8_t  Hardware::sw1Pos()  { return s_sw1;    }
uint8_t  Hardware::sw2Pos()  { return s_sw2;    }
uint16_t Hardware::potRaw()  { return s_potRaw; }
