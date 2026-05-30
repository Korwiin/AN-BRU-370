#include "encoder.h"
#include "pins.h"

static const int8_t k_lookup[16] = {
  0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0
};

static int      s_lastState   = 3;
static long     s_stepCount   = 0;
static unsigned long s_lastEncTime = 0;

static bool s_lastBtnState  = HIGH;
static unsigned long s_lastDebounce = 0;
static unsigned long s_btnDownTime  = 0;
static bool s_btnActive     = false;
static bool s_shortPress    = false;
static bool s_longPress     = false;
static bool s_lpFired       = false;

constexpr unsigned long kDebouncMs   = 30;
constexpr unsigned long kLongPressMs = 600;
constexpr unsigned long kCooldownMs  = 40;
constexpr int           kDivider     = 4;

void Encoder::begin() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW,  INPUT_PULLUP);
  s_lastState = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
}

int8_t Encoder::readDelta() {
  s_shortPress = false;
  s_longPress  = false;

  // Rotation
  int cur = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  if (cur != s_lastState) {
    int8_t d = k_lookup[(s_lastState << 2) | cur];
    if (d != 0) {
      unsigned long now = millis();
      if (now - s_lastEncTime > kCooldownMs) {
        s_stepCount += d;
        s_lastEncTime = now;
      }
    }
    s_lastState = cur;
  }

  int8_t out = 0;
  if (abs(s_stepCount) >= kDivider) {
    out = (s_stepCount > 0) ? 1 : -1;
    s_stepCount = 0;
  }

  // Button debounce
  bool reading = (digitalRead(PIN_ENC_SW) == LOW);
  if (reading != s_lastBtnState) {
    s_lastDebounce = millis();
    s_lastBtnState = reading;
  }

  if (millis() - s_lastDebounce > kDebouncMs) {
    if (reading && !s_btnActive) {
      s_btnActive  = true;
      s_btnDownTime = millis();
      s_lpFired    = false;
    } else if (!reading && s_btnActive) {
      if (millis() - s_btnDownTime < kLongPressMs) s_shortPress = true;
      s_btnActive = false;
    }
  }

  if (s_btnActive && !s_lpFired &&
      millis() - s_btnDownTime > kLongPressMs) {
    s_longPress = true;
    s_lpFired   = true;
  }
  return out;
}

bool Encoder::shortPressed() { return s_shortPress; }
bool Encoder::longPressed()  { return s_longPress;  }

void Encoder::flush() {
  s_stepCount  = 0;
  s_shortPress = false;
  s_longPress  = false;
  s_lpFired    = false;
}
