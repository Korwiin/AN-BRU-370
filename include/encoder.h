#pragma once
#include <Arduino.h>

namespace Encoder {
  void begin();

  // Call every loop(). Updates internal state.
  // Returns +1 / -1 for rotation, 0 if no movement.
  int8_t readDelta();

  // True on the loop() cycle the button short-press is detected (released < 600 ms).
  bool shortPressed();

  // True on the loop() cycle a long press fires (held > 600 ms).
  bool longPressed();

  // Reset accumulated rotation and button state. Call before entering a blocking input loop.
  void flush();
}
