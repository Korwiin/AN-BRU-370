#include "lvgl_port.h"
#include "display.h"
#include <Arduino.h>
#include <lvgl.h>
#include "esp_heap_caps.h"

namespace {
  constexpr int BUF_LINES = 60;  // partial render buffer height

  void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_draw_bitmap(Display::panel(),
                              area->x1, area->y1, area->x2 + 1, area->y2 + 1,
                              px_map);
    lv_display_flush_ready(disp);  // draw_bitmap copies synchronously
  }
}

namespace LvglPort {

bool begin() {
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  size_t bufBytes = Display::WIDTH * BUF_LINES * 2;
  void* buf1 = heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
  void* buf2 = heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM);
  if (!buf1 || !buf2) return false;

  lv_display_t* disp = lv_display_create(Display::WIDTH, Display::HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf1, buf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  return true;
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last >= 5) {
    last = millis();
    lv_timer_handler();
  }
}

}  // namespace LvglPort
