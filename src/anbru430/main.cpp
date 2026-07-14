#include <Arduino.h>
#include <USB.h>
#include <lvgl.h>
#include "config.h"
#include "display.h"
#include "lvgl_port.h"

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
  lv_label_set_text(title, "ANBRU-430");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t* ver = lv_label_create(scr);
  lv_label_set_text(ver, "fw v" FIRMWARE_VERSION);
  lv_obj_set_style_text_color(ver, lv_color_hex(0xB0B8C0), 0);
  lv_obj_align(ver, LV_ALIGN_CENTER, 0, 20);

  Serial.println("lvgl up");
}

void loop() {
  LvglPort::loop();
}
