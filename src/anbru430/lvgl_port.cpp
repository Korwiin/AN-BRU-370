#include "lvgl_port.h"
#include "display.h"
#include <Arduino.h>
#include <lvgl.h>
#include "esp_lcd_panel_rgb.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
  SemaphoreHandle_t s_vsyncSem = nullptr;

  // Fires on the RGB panel's hardware VSYNC interrupt (still delivered with
  // bounce buffers enabled — confirmed in the installed esp_lcd_panel_rgb ISR,
  // gated only on the VSYNC status bit, independent of bounce-buffer config).
  bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t* edata, void* user_ctx) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_vsyncSem, &woken);
    return woken == pdTRUE;
  }

  void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    if (lv_display_flush_is_last(disp)) {
      xSemaphoreTake(s_vsyncSem, pdMS_TO_TICKS(100));  // never hang the UI on a missed vsync
      // px_map points at one of the panel's own framebuffers (DIRECT mode) —
      // draw_bitmap flips scanout to it without copying.
      esp_lcd_panel_draw_bitmap(Display::panel(), 0, 0, Display::WIDTH, Display::HEIGHT, px_map);
    }
    lv_display_flush_ready(disp);
  }
}

namespace LvglPort {

bool begin() {
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  void* fb0 = nullptr;
  void* fb1 = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(Display::panel(), 2, &fb0, &fb1) != ESP_OK) return false;
  if (!fb0 || !fb1) return false;

  s_vsyncSem = xSemaphoreCreateBinary();
  if (!s_vsyncSem) return false;

  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  if (esp_lcd_rgb_panel_register_event_callbacks(Display::panel(), &cbs, nullptr) != ESP_OK) return false;

  lv_display_t* disp = lv_display_create(Display::WIDTH, Display::HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, fb0, fb1, Display::WIDTH * Display::HEIGHT * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
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
