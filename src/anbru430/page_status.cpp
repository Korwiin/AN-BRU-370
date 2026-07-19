#include "ui.h"
#include "config.h"
#include "wifi_mgr.h"
#include "dcs_bios.h"
#include <WiFi.h>

namespace {
  lv_obj_t* s_wifi = nullptr;
  lv_obj_t* s_dcs  = nullptr;
  lv_obj_t* s_mem  = nullptr;
  lv_obj_t* s_up   = nullptr;
}

namespace PageStatus {

void build(lv_obj_t* parent) {
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(parent, 24, 0);
  lv_obj_set_style_pad_gap(parent, 14, 0);
  UI::makeLabel(parent, "AN/BRU-430   fw v" FIRMWARE_VERSION,
                &lv_font_montserrat_28, UI::colAccent());
  s_wifi = UI::makeLabel(parent, "WiFi: ...", &lv_font_montserrat_20, UI::colText());
  s_dcs  = UI::makeLabel(parent, "DCS: ...",  &lv_font_montserrat_20, UI::colText());
  s_mem  = UI::makeLabel(parent, "Mem: ...",  &lv_font_montserrat_20, UI::colText());
  s_up   = UI::makeLabel(parent, "Up: ...",   &lv_font_montserrat_20, UI::colText());
}

void refresh() {
  if (WifiMgr::isConnected()) {
    lv_label_set_text_fmt(s_wifi, "WiFi: %s   %s   %d dBm",
                          WifiMgr::activeSSID(),
                          WiFi.localIP().toString().c_str(),
                          (int)WiFi.RSSI());
  } else {
    lv_label_set_text_fmt(s_wifi, "WiFi: not connected (%s)", WifiMgr::activeSSID());
  }
  lv_label_set_text_fmt(s_dcs, "DCS: %s",
                        DcsBios::isConnected() ? "connected" : "waiting");
  lv_label_set_text_fmt(s_mem, "Heap Free: %u   PSRAM: %u",
                        (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  lv_label_set_text_fmt(s_up, "Uptime: %lu min", millis() / 60000UL);
}

}  // namespace PageStatus
