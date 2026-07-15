#include "ui.h"
#include "settings_internal.h"
#include "config.h"
#include "ota.h"
#include "wifi_mgr.h"
#include "lvgl_port.h"
#include <Arduino.h>
#include "esp32-hal-tinyusb.h"

namespace {
  lv_obj_t* s_sub        = nullptr;
  lv_obj_t* s_msg        = nullptr;
  lv_obj_t* s_installBtn = nullptr;
  OTA::CheckResult s_result;
  int s_lastPct = -100;
  lv_obj_t* s_updLbl = nullptr;
  lv_obj_t* s_updBar = nullptr;
  volatile bool s_cancelFlag = false;

  // OTA download contends with the RGB-DMA framebuffer for PSRAM bandwidth —
  // redraw only a label + bar, at >=5% steps, on an otherwise static black screen.
  void progCb(int p) {
    if (p - s_lastPct < 5 && p != 100) return;
    s_lastPct = p;
    lv_label_set_text_fmt(s_updLbl, "Downloading  %d%%", p);
    lv_bar_set_value(s_updBar, p, LV_ANIM_OFF);
    lv_refr_now(nullptr);
  }

  void doInstall(lv_event_t*) {
    lv_obj_t* m = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m, 800, 480);
    lv_obj_set_pos(m, 0, 0);
    UI::stripPanel(m);
    lv_obj_set_style_bg_color(m, lv_color_black(), 0);
    lv_obj_t* t = UI::makeLabel(m, "UPDATING FIRMWARE\nDO NOT POWER OFF",
                                &lv_font_montserrat_28, UI::colWarn());
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -90);
    s_updLbl = UI::makeLabel(m, "Starting...", &lv_font_montserrat_20, UI::colText());
    lv_obj_align(s_updLbl, LV_ALIGN_CENTER, 0, 0);
    s_updBar = lv_bar_create(m);
    lv_obj_set_size(s_updBar, 600, 20);
    lv_obj_align(s_updBar, LV_ALIGN_CENTER, 0, 50);
    lv_bar_set_range(s_updBar, 0, 100);
    lv_refr_now(nullptr);

    s_lastPct = -100;
    OTA::perform(s_result.url, progCb);   // restarts the device on success
    lv_obj_delete(m);
    lv_label_set_text_fmt(s_msg, "Update failed: %s", OTA::performError());
    lv_obj_remove_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);  // allow retry
  }

  void doCheck(lv_event_t*) {
    lv_obj_add_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);
    if (!WifiMgr::isConnected()) {
      lv_label_set_text(s_msg, "No WiFi");
      return;
    }
    lv_label_set_text(s_msg, "Checking...");
    lv_refr_now(nullptr);
    s_result = OTA::check();              // blocking 1-3 s
    if (s_result.error[0]) {
      lv_label_set_text_fmt(s_msg, "Check failed: %s", s_result.error);
      return;
    }
    if (!s_result.available) {
      lv_label_set_text_fmt(s_msg, "Up to date (v%s)", FIRMWARE_VERSION);
      return;
    }
    lv_label_set_text_fmt(s_msg, "v%u.%02u available  (current v%s)",
                          s_result.versionInt / 100, s_result.versionInt % 100,
                          FIRMWARE_VERSION);
    lv_obj_remove_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);
  }

  void doUsbFlash(lv_event_t*) {
#ifdef DEV_BUILD
    // With HWCDC live on USB-Serial/JTAG, usb_persist_restart's PHY handoff
    // leaves the bus detached until RST (Brew370 finding 2026-07-03); dev
    // builds flash button-free anyway.
    lv_label_set_text(s_msg, "USB Flash unavailable in dev build");
#else
    s_cancelFlag = false;
    lv_obj_t* m = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m, 800, 480);
    lv_obj_set_pos(m, 0, 0);
    UI::stripPanel(m);
    lv_obj_t* lbl = UI::makeLabel(m, "", &lv_font_montserrat_28, UI::colWarn());
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);
    lv_obj_t* c = UI::makeButton(m, "Cancel",
        [](lv_event_t*) { s_cancelFlag = true; }, nullptr);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 70);
    for (int secs = 5; secs > 0; secs--) {
      lv_label_set_text_fmt(lbl, "USB flash mode in %d...", secs);
      unsigned long t0 = millis();
      while (millis() - t0 < 1000UL) {
        LvglPort::loop();
        if (s_cancelFlag) { lv_obj_delete(m); return; }
        delay(10);
      }
    }
    lv_obj_delete(c);
    lv_label_set_text(lbl, "USB FLASH MODE\nFlash from PC, then press RST");
    lv_refr_now(nullptr);
    delay(200);
    usb_persist_restart(RESTART_BOOTLOADER);
#endif
  }
}

namespace SettingsUpdate {

void build() {
  s_sub = SettingsInt::makeSub();
  lv_obj_t* t = UI::makeLabel(s_sub, "FIRMWARE   v" FIRMWARE_VERSION,
                              &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 16);
  s_msg = UI::makeLabel(s_sub, "", &lv_font_montserrat_20, UI::colWarn());
  lv_obj_align(s_msg, LV_ALIGN_TOP_LEFT, 16, 70);

  lv_obj_t* b = UI::makeButton(s_sub, "Check Update", doCheck, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 16, -84);
  s_installBtn = UI::makeButton(s_sub, "Install", doInstall, nullptr);
  lv_obj_set_style_bg_color(s_installBtn, UI::colAccent(), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(s_installBtn, 0), lv_color_black(), 0);
  lv_obj_align(s_installBtn, LV_ALIGN_BOTTOM_MID, 0, -84);
  lv_obj_add_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);
  b = UI::makeButton(s_sub, "USB Flash", doUsbFlash, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -84);
  b = UI::makeButton(s_sub, "Back",
      [](lv_event_t*) { SettingsInt::showRoot(); }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void open() {
  lv_label_set_text(s_msg, "");
  lv_obj_add_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);
  SettingsInt::showSub(s_sub);
}

}  // namespace SettingsUpdate
