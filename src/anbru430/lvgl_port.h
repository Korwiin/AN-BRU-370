#pragma once

namespace LvglPort {
  bool begin();   // requires Display::begin() already done
  void loop();    // call from loop(); runs lv_timer_handler at its own pace
}
