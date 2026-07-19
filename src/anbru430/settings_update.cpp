#include "ui.h"
#include "settings_internal.h"
#include "config.h"
#include "ota.h"
#include "wifi_mgr.h"
#include "display.h"
#include <Arduino.h>
#include "esp32-hal-tinyusb.h"
#include "esp_lcd_panel_rgb.h"

namespace {
  lv_obj_t* s_sub        = nullptr;
  lv_obj_t* s_msg        = nullptr;
  lv_obj_t* s_installBtn = nullptr;
  OTA::CheckResult s_result;

  lv_obj_t*   s_flashModal     = nullptr;
  lv_obj_t*   s_flashLbl       = nullptr;
  lv_obj_t*   s_flashCancelBtn = nullptr;
  lv_timer_t* s_flashTimer     = nullptr;
  int         s_flashSecs      = 0;

  void doInstall(lv_event_t*) {
    lv_obj_t* m = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m, Display::WIDTH, Display::HEIGHT);
    lv_obj_set_pos(m, 0, 0);
    UI::stripPanel(m);
    lv_obj_set_style_bg_color(m, lv_color_black(), 0);
    lv_obj_t* t = UI::makeLabel(m, "UPDATING FIRMWARE\nDO NOT POWER OFF",
                                &lv_font_montserrat_28, UI::colWarn());
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -90);
    lv_obj_t* d = UI::makeLabel(m, "Downloading... this takes about a minute.",
                                &lv_font_montserrat_20, UI::colText());
    lv_obj_align(d, LV_ALIGN_CENTER, 0, 0);
    lv_refr_now(nullptr);

    // Zero redraws during the flash write: esp_ota_write periodically disables
    // the external-memory cache (IDF RGB-LCD limitation), and any LVGL redraw
    // in that window glitches the RGB scanout. Screen stays static until done.
    if (!OTA::perform(s_result.url, nullptr)) {   // restarts the device on success
      // The cache-disable windows above can leave the RGB scanout permanently
      // shifted; restart the panel before resuming the live UI.
      esp_lcd_rgb_panel_restart(Display::panel());
      lv_obj_delete(m);
      if (WifiMgr::isConnected()) {
        lv_label_set_text_fmt(s_msg, "Update failed: %s", OTA::performError());
        lv_obj_remove_flag(s_installBtn, LV_OBJ_FLAG_HIDDEN);  // allow retry
      } else {
        lv_label_set_text_fmt(s_msg, "Update failed: %s (no WiFi)", OTA::performError());
        // Install stays hidden — user must re-run Check Update once WiFi is back.
      }
    }
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

  void flashCancel(lv_event_t*) {
    if (s_flashTimer) { lv_timer_delete(s_flashTimer); s_flashTimer = nullptr; }
    if (s_flashModal) { lv_obj_delete(s_flashModal); s_flashModal = nullptr; }
  }

  void flashTick(lv_timer_t*) {
    if (--s_flashSecs <= 0) {
      lv_timer_delete(s_flashTimer);
      s_flashTimer = nullptr;
      lv_obj_delete(s_flashCancelBtn);
      s_flashCancelBtn = nullptr;
      lv_label_set_text(s_flashLbl, "USB FLASH MODE\nFlash from PC, then press RST");
      lv_refr_now(nullptr);
      delay(200);
      usb_persist_restart(RESTART_BOOTLOADER);
      return;
    }
    lv_label_set_text_fmt(s_flashLbl, "USB flash mode in %d...", s_flashSecs);
  }

  void doUsbFlash(lv_event_t*) {
#ifdef DEV_BUILD
    // With HWCDC live on USB-Serial/JTAG, usb_persist_restart's PHY handoff
    // leaves the bus detached until RST (Brew370 finding 2026-07-03); dev
    // builds flash button-free anyway.
    lv_label_set_text(s_msg, "USB Flash unavailable in dev build");
#else
    s_flashSecs = 5;
    s_flashModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_flashModal, Display::WIDTH, Display::HEIGHT);
    lv_obj_set_pos(s_flashModal, 0, 0);
    UI::stripPanel(s_flashModal);
    s_flashLbl = UI::makeLabel(s_flashModal, "USB flash mode in 5...", &lv_font_montserrat_28, UI::colWarn());
    lv_obj_set_style_text_align(s_flashLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_flashLbl, LV_ALIGN_CENTER, 0, -40);
    s_flashCancelBtn = UI::makeButton(s_flashModal, "Cancel", flashCancel, nullptr);
    lv_obj_align(s_flashCancelBtn, LV_ALIGN_CENTER, 0, 70);
    s_flashTimer = lv_timer_create(flashTick, 1000, nullptr);
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
