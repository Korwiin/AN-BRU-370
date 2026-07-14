#pragma once
#include <Arduino.h>
#include "esp_lcd_panel_ops.h"

namespace Display {
  bool begin();                       // CH422G reset/backlight + esp_lcd RGB init
  esp_lcd_panel_handle_t panel();
  constexpr uint16_t WIDTH  = 800;
  constexpr uint16_t HEIGHT = 480;
}
