#include "ui.h"
#include "settings_internal.h"
#include "wifi_mgr.h"
#include <Arduino.h>
#include <WiFi.h>

namespace {
  lv_obj_t* s_subWifi    = nullptr;
  lv_obj_t* s_stat       = nullptr;
  lv_obj_t* s_subSecrets = nullptr;
  lv_obj_t* s_credLbl    = nullptr;
  void (*s_connectCb)() = nullptr;
  void (*s_bleReqCb)()  = nullptr;

  // Minimal confirm modal (Yes runs the action; Cancel just closes)
  void (*s_confirmYes)() = nullptr;
  lv_obj_t* s_confirm = nullptr;

  void confirmShow(const char* text, const char* yesLabel, void (*onYes)()) {
    s_confirmYes = onYes;
    s_confirm = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_confirm, 560, 240);
    lv_obj_center(s_confirm);
    UI::stripPanel(s_confirm);
    lv_obj_set_style_bg_color(s_confirm, UI::colPanel(), 0);
    lv_obj_set_style_border_width(s_confirm, 2, 0);
    lv_obj_set_style_border_color(s_confirm, UI::colAccent(), 0);
    lv_obj_t* l = UI::makeLabel(s_confirm, text, &lv_font_montserrat_20, UI::colText());
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t* b = UI::makeButton(s_confirm, yesLabel, [](lv_event_t*) {
      void (*f)() = s_confirmYes;
      lv_obj_delete_async(s_confirm);
      s_confirm = nullptr;
      if (f) f();
    }, nullptr);
    lv_obj_set_width(b, 200);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 24, -20);
    b = UI::makeButton(s_confirm, "Cancel", [](lv_event_t*) {
      lv_obj_delete_async(s_confirm);
      s_confirm = nullptr;
    }, nullptr);
    lv_obj_set_width(b, 200);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -24, -20);
  }

  void refreshStat() {
    if (WifiMgr::isConnected()) {
      lv_label_set_text_fmt(s_stat, "Connected: %s   %s   %d dBm",
                            WifiMgr::activeSSID(),
                            WiFi.localIP().toString().c_str(),
                            (int)WiFi.RSSI());
    } else {
      lv_label_set_text_fmt(s_stat, "Not connected (%s)", WifiMgr::activeSSID());
    }
  }

  void refreshCreds() {
    static char ssid[64];
    uint8_t passStatus = 0;
    WifiMgr::nvsCredentials(ssid, sizeof(ssid), &passStatus);
    static const char* k_pass[] = { "(none)", "(empty)", "(set)" };
    lv_label_set_text_fmt(s_credLbl, "SSID: %s\nPassword: %s",
                          ssid[0] ? ssid : "(none)",
                          k_pass[passStatus <= 2 ? passStatus : 0]);
  }

  void doZeroize() {
    WifiMgr::clearOverride();
    lv_obj_t* m = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m, 800, 480);
    lv_obj_set_pos(m, 0, 0);
    UI::stripPanel(m);
    lv_obj_t* l = UI::makeLabel(m, "Credentials erased.\nRebooting...",
                                &lv_font_montserrat_28, UI::colWarn());
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    lv_refr_now(nullptr);
    delay(800);
    ESP.restart();
  }
}

namespace SettingsWifi {

void build() {
  // WiFi sub
  s_subWifi = SettingsInt::makeSub();
  lv_obj_t* t = UI::makeLabel(s_subWifi, "WIFI", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 16);
  s_stat = UI::makeLabel(s_subWifi, "...", &lv_font_montserrat_20, UI::colText());
  lv_obj_align(s_stat, LV_ALIGN_TOP_LEFT, 16, 70);
  lv_obj_t* b = UI::makeButton(s_subWifi, "Connect", [](lv_event_t*) {
    if (s_connectCb) s_connectCb();
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 16, -16);
  b = UI::makeButton(s_subWifi, "Secrets", [](lv_event_t*) {
    refreshCreds();
    SettingsInt::showSub(s_subSecrets);
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -16);
  b = UI::makeButton(s_subWifi, "Back",
      [](lv_event_t*) { SettingsInt::showRoot(); }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -16);

  // Secrets sub
  s_subSecrets = SettingsInt::makeSub();
  t = UI::makeLabel(s_subSecrets, "SECRETS", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 16);
  s_credLbl = UI::makeLabel(s_subSecrets, "...", &lv_font_montserrat_20, UI::colText());
  lv_obj_align(s_credLbl, LV_ALIGN_TOP_LEFT, 16, 70);
  b = UI::makeButton(s_subSecrets, "BLE Terminal", [](lv_event_t*) {
    // BLE can't advertise while TinyUSB is up (IDF 5.5.4) — reboot into setup mode.
    confirmShow("BLE setup requires a reboot.\nThe device restarts into setup mode.",
                "Reboot", []() { if (s_bleReqCb) s_bleReqCb(); });
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 16, -84);
  b = UI::makeButton(s_subSecrets, "Zeroize", [](lv_event_t*) {
    confirmShow("Erase saved WiFi credentials?", "Erase", doZeroize);
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -84);
  b = UI::makeButton(s_subSecrets, "Scan", [](lv_event_t*) {
    lv_label_set_text(s_credLbl, "Scan: not implemented");
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 16, -12);
  b = UI::makeButton(s_subSecrets, "Back", [](lv_event_t*) {
    refreshStat();
    SettingsInt::showSub(s_subWifi);
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
}

void open() {
  refreshStat();
  SettingsInt::showSub(s_subWifi);
}

void setConnectCb(void (*cb)())    { s_connectCb = cb; }
void setBleRequestCb(void (*cb)()) { s_bleReqCb = cb; }

}  // namespace SettingsWifi
