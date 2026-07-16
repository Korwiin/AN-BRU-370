#pragma once
#include <stdint.h>

namespace Touch {
  bool begin();
  uint32_t lastTouchMs();               // millis() of last physical press; 0 = never
  void swallowUntilRelease();           // eat LVGL presses until the finger lifts (arm at sleep entry)
  void clearSwallow();                  // non-touch wake (DCS/alert-driven): no finger down to release
  void inject(uint16_t x, uint16_t y);  // synthesize an 80 ms tap (test shell)
}
