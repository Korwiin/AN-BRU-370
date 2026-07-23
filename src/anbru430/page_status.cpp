#include "ui.h"
#include "config.h"
#include "wifi_mgr.h"
#include "dcs_bios.h"
#include <WiFi.h>
#include <string.h>

namespace {
  lv_obj_t* s_wifi = nullptr;
  lv_obj_t* s_dcs  = nullptr;
  lv_obj_t* s_mem  = nullptr;
  lv_obj_t* s_up   = nullptr;

  // refresh() redraw gate — last-applied text per label, mirrors Alerts CHAFF gate.
  char s_lastWifi[96] = "";
  char s_lastDcs[24]  = "";
  char s_lastMem[48]  = "";
  char s_lastUp[32]   = "";

  void setIfChanged(lv_obj_t* label, char* cache, size_t cacheSize, const char* text) {
    if (strcmp(cache, text) == 0) return;
    strlcpy(cache, text, cacheSize);
    lv_label_set_text(label, text);
  }
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
  char buf[96];
  if (WifiMgr::isConnected()) {
    snprintf(buf, sizeof(buf), "WiFi: %s   %s   %d dBm",
             WifiMgr::activeSSID(),
             WiFi.localIP().toString().c_str(),
             (int)WiFi.RSSI());
  } else {
    snprintf(buf, sizeof(buf), "WiFi: not connected (%s)", WifiMgr::activeSSID());
  }
  setIfChanged(s_wifi, s_lastWifi, sizeof(s_lastWifi), buf);

  snprintf(buf, sizeof(buf), "DCS: %s", DcsBios::isConnected() ? "connected" : "waiting");
  setIfChanged(s_dcs, s_lastDcs, sizeof(s_lastDcs), buf);

  snprintf(buf, sizeof(buf), "Heap Free: %u   PSRAM: %u",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  setIfChanged(s_mem, s_lastMem, sizeof(s_lastMem), buf);

  snprintf(buf, sizeof(buf), "Uptime: %lu min", millis() / 60000UL);
  setIfChanged(s_up, s_lastUp, sizeof(s_lastUp), buf);
}

}  // namespace PageStatus
