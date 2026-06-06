#include "hardware.h"
#include "hid.h"
#include "pins.h"

static uint8_t       s_sw1       = 0xFF;
static uint8_t       s_sw2       = 0xFF;
static uint8_t       s_pending1  = 0xFF;
static uint8_t       s_pending2  = 0xFF;
static unsigned long s_debounce1 = 0;
static unsigned long s_debounce2 = 0;
static unsigned long s_lastSend  = 0;

static constexpr unsigned long DEBOUNCE_MS = 25;

static uint8_t readSwitch(uint8_t pinA, uint8_t pinB) {
  bool a = (digitalRead(pinA) == LOW);
  bool b = (digitalRead(pinB) == LOW);
  if  (a && !b) return 0;
  if (!a && !b) return 1;  // center — no throw connected
  if (!a &&  b) return 2;
  return 1;                // both LOW = wiring fault, default center
}

// Gamepad button mapping (one bit held per switch position):
//   Bit 0 (btn 1) = SW1 pos 0 down   — PITCH ATT HOLD
//   Bit 1 (btn 2) = SW1 pos 1 center — A/P OFF
//   Bit 2 (btn 3) = SW1 pos 2 up     — PITCH ALT HOLD
//   Bit 3 (btn 4) = SW2 pos 0 down   — ROLL STRG SEL
//   Bit 4 (btn 5) = SW2 pos 1 center — ROLL ATT HOLD
//   Bit 5 (btn 6) = SW2 pos 2 up     — ROLL HDG SEL
// Z axis fixed at -127 so DCS auto-assigned throttle starts at 0% not 50%.
// X/Y/Rz left at 0 (center) — auto-assigned to pitch/roll/rudder, center is correct.
static bool pushGamepad(uint8_t sw1, uint8_t sw2) {
  uint32_t btns = 0;
  if (sw1 == 0) btns |= (1u << 0);
  if (sw1 == 1) btns |= (1u << 1);
  if (sw1 == 2) btns |= (1u << 2);
  if (sw2 == 0) btns |= (1u << 3);
  if (sw2 == 1) btns |= (1u << 4);
  if (sw2 == 2) btns |= (1u << 5);

  return HID::Gamepad.send(0, 0, -127, 0, 0, 0, HAT_CENTER, btns);
}

void Hardware::begin() {
  pinMode(PIN_SW1_A, INPUT_PULLUP);
  pinMode(PIN_SW1_B, INPUT_PULLUP);
  pinMode(PIN_SW2_A, INPUT_PULLUP);
  pinMode(PIN_SW2_B, INPUT_PULLUP);

  s_sw1      = readSwitch(PIN_SW1_A, PIN_SW1_B);
  s_sw2      = readSwitch(PIN_SW2_A, PIN_SW2_B);
  s_pending1 = s_sw1;
  s_pending2 = s_sw2;
  pushGamepad(s_sw1, s_sw2);
}

void Hardware::update() {
  uint8_t       raw1 = readSwitch(PIN_SW1_A, PIN_SW1_B);
  uint8_t       raw2 = readSwitch(PIN_SW2_A, PIN_SW2_B);
  unsigned long now  = millis();

  if (raw1 != s_pending1) { s_pending1 = raw1; s_debounce1 = now; }
  if (raw2 != s_pending2) { s_pending2 = raw2; s_debounce2 = now; }

  bool changed = false;
  if ((now - s_debounce1 >= DEBOUNCE_MS) && s_pending1 != s_sw1) { s_sw1 = s_pending1; changed = true; }
  if ((now - s_debounce2 >= DEBOUNCE_MS) && s_pending2 != s_sw2) { s_sw2 = s_pending2; changed = true; }

  bool periodic = (now - s_lastSend >= 2000UL);

  if (changed || periodic) {
    if (pushGamepad(s_sw1, s_sw2)) s_lastSend = now;
  }
}

void Hardware::forceSync() {
  s_lastSend = 0;  // next update() fires immediately
}

uint8_t Hardware::sw1Pos() { return s_sw1; }
uint8_t Hardware::sw2Pos() { return s_sw2; }
