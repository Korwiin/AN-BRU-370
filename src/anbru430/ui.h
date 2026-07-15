#pragma once
#include <Arduino.h>
#include <lvgl.h>

// ANBRU-430 LVGL UI. Bottom nav bar + page container. Pages are built once at
// UI::begin() and shown/hidden by UI::showPage(). Alerts live on lv_layer_top()
// (cover everything incl. nav); the dim overlay lives on lv_layer_sys() and
// passes touches through. Page modules are dumb widget builders — all DCS and
// app logic stays in main.cpp.

namespace UI {
  enum Page : uint8_t { PAGE_HOME, PAGE_MACROS, PAGE_SETTINGS, PAGE_STATUS, PAGE_COUNT };

  void begin();               // call after LvglPort::begin(), before Touch::begin()
  void showPage(Page p);      // resets Settings to its root list when navigating away
  Page currentPage();

  void setDimLevel(int pct);  // 0..80 — black overlay opacity (cosmetic; no power saving)

  // Palette
  inline lv_color_t colBg()     { return lv_color_hex(0x101418); }
  inline lv_color_t colPanel()  { return lv_color_hex(0x202830); }
  inline lv_color_t colText()   { return lv_color_hex(0xE0E4E8); }
  inline lv_color_t colAccent() { return lv_color_hex(0x00FF66); }
  inline lv_color_t colWarn()   { return lv_color_hex(0xFFC040); }
  inline lv_color_t colAlert()  { return lv_color_hex(0xFF3020); }

  // Shared widget helpers
  lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user);
  lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color);
  void stripPanel(lv_obj_t* obj);   // bg colBg, no border/radius/scroll — for containers
}

namespace PageHome {
  enum State : uint8_t { NO_CREDS, WIFI_CONNECTING, WIFI_FAILED, WAITING_DCS, AIRCRAFT, STATE_COUNT };
  void build(lv_obj_t* parent);
  void setState(State s);
  void setSsid(const char* ssid);        // WIFI_CONNECTING caption
  void setFuel(uint32_t lbs);            // AIRCRAFT readout
  void setRetryCb(void (*cb)());         // WIFI_FAILED [Retry]
  void setBleSetupCb(void (*cb)());      // NO_CREDS [Reboot into BLE Setup]
}

namespace PageMacros {
  void build(lv_obj_t* parent);
}

namespace PageStatus {
  void build(lv_obj_t* parent);
  void refresh();                        // call ~1 Hz while the page is visible
}

namespace PageSettings {
  void build(lv_obj_t* parent);          // also builds SettingsWifi/Update/Mouse subs
  void reset();                          // back to root list (called on nav-away)
  void setDimPtr(int* dim);              // main's live dim level (0..80)
  void setSleepSecsPtr(int* secs);       // main's live sleep timeout
}

namespace SettingsWifi {
  void build();
  void open();
  void setConnectCb(void (*cb)());       // main: beginConnect(false) + WIFI_CONNECTING
  void setBleRequestCb(void (*cb)());    // main: set NVS blereq flag + reboot
}

namespace SettingsUpdate {
  void build();
  void open();
}

namespace SettingsMouse {
  void build();
  void open();
  void setScreenPtrs(int* w, int* h);    // main's PC screen dims (HID coordinate space)
}

namespace Alerts {
  enum Kind : uint8_t { NONE, MASTER_CAUTION, STORES_CONFIG, MISSILE, CHAFF,
                        NOT_READY_ALERT, SETUP_PROGRESS };
  void show(Kind k, bool flashOn);       // idempotent; builds the modal lazily
  void setChaff(const char* chaffStr);   // before show(CHAFF, ...)
  void setSetupProgress(uint8_t step, uint8_t maxStep, bool blinkOn);  // shows itself
  void hide();
  Kind current();
  void setTapCb(void (*cb)());           // whole-modal tap (MC reset / SC toggle / dispense)
  void setNotReadyCbs(void (*runSetup)(), void (*mwsOn)());
}
