#include <Arduino.h>
#include <USB.h>
#include <lvgl.h>
#include "config.h"
#include "display.h"
#include "lvgl_port.h"
#include "touch.h"

void setup() {
  USB.manufacturerName("E4 Mafia");
  USB.productName(USB_PRODUCT_NAME);
  USB.PID(USB_PID);
  USB.firmwareVersion(FIRMWARE_VERSION_INT);
  USB.begin();

  Serial.begin(115200);
  while (!Serial && millis() < 2000) { }
  Serial.println("=== ANBRU-430 boot ===");

  if (!Display::begin() || !LvglPort::begin()) {
    Serial.println("DISPLAY/LVGL INIT FAILED");
    return;
  }

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "ANBRU-430  fw v" FIRMWARE_VERSION "  touch test");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  static lv_obj_t* coords = lv_label_create(scr);
  lv_label_set_text(coords, "touch the screen");
  lv_obj_set_style_text_color(coords, lv_color_hex(0xB0B8C0), 0);
  lv_obj_align(coords, LV_ALIGN_TOP_MID, 0, 32);

  static lv_obj_t* dot = lv_obj_create(scr);
  lv_obj_set_size(dot, 40, 40);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(0xFF3030), 0);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

  if (!Touch::begin()) {
    lv_label_set_text(coords, "GT911 NOT FOUND");
  } else {
    // Poll the LVGL pointer each 50 ms and mirror it into label + dot
    lv_timer_create([](lv_timer_t*) {
      lv_indev_t* indev = lv_indev_get_next(nullptr);
      if (!indev) return;
      lv_point_t p;
      lv_indev_get_point(indev, &p);
      if (lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
        lv_label_set_text_fmt(coords, "x=%d  y=%d", (int)p.x, (int)p.y);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(dot, p.x - 20, p.y - 20);
      }
    }, 50, nullptr);
  }

  Serial.println("lvgl up");
}

void loop() {
  LvglPort::loop();
}
