#pragma once
#include <stdint.h>

namespace Touch {
  bool begin();
  uint32_t lastTouchMs();               // millis() of last physical press; 0 = never
  void swallowUntilRelease();           // eat LVGL presses until the finger lifts (sleep wake)
  void inject(uint16_t x, uint16_t y);  // synthesize an 80 ms tap (test shell)
}
