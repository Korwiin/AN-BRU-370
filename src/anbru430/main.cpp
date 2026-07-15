#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>
#include "esp32-hal-tinyusb.h"  // usb_persist_restart — reboot into ROM download mode
#include "esp_bt.h"             // esp_bt_controller_get_status — BLE setup diagnostic
#include "config.h"
#include "display.h"
#include "lvgl_port.h"
#include "touch.h"
#include "wifi_mgr.h"
#include "dcs_bios.h"
#include "hid.h"
#include "macros.h"
#include "ota.h"
#include "shell.h"

// --- prefs (same NVS keys as Brew370 so mouse tuning carries over) ---
static int s_screenW = 1920;
static int s_screenH = 1080;

static void loadPrefs() {
  Preferences prefs;
  prefs.begin("brew", true);
  s_screenW = prefs.getInt("scrW", 1920);
  s_screenH = prefs.getInt("scrH", 1080);
  mouseParams[0] = prefs.getInt("apxX",  s_screenW / 4);
  mouseParams[1] = prefs.getInt("apxY",  s_screenH / 54);
  mouseParams[2] = prefs.getInt("amcX2", s_screenW / 2);
  mouseParams[3] = prefs.getInt("amcY2", s_screenH / 2);
  mouseParams[4] = prefs.getInt("lbX2",  s_screenW / 2);
  mouseParams[5] = prefs.getInt("lbY2",  s_screenH / 2);
  mouseParams[6] = prefs.getInt("cdrpX", s_screenW / 5);
  mouseParams[7] = prefs.getInt("cdrpY", s_screenH / 2);
  prefs.end();
}

// --- status screen widgets ---
static lv_obj_t* s_wifiLbl;
static lv_obj_t* s_dcsLbl;
static lv_obj_t* s_fuelLbl;
static lv_obj_t* s_msgLbl;
static lv_obj_t* s_heapLbl;

static void msg(const char* text) {
  lv_label_set_text(s_msgLbl, text);
  lv_refr_now(nullptr);  // repaint immediately — used inside blocking flows
}

static void onUsbFlash(lv_event_t*) {
  msg("Entering USB flash mode...");
  delay(400);
  usb_persist_restart(RESTART_BOOTLOADER);
}

static void onCheckUpdate(lv_event_t*) {
  msg("Checking for update...");
  OTA::CheckResult r = OTA::check();          // blocking 1-3 s
  if (r.error[0]) { lv_label_set_text_fmt(s_msgLbl, "Check failed: %s", r.error); return; }
  if (!r.available) {
    lv_label_set_text_fmt(s_msgLbl, "Up to date (v%s)", FIRMWARE_VERSION);
    return;
  }
  lv_label_set_text_fmt(s_msgLbl, "Updating to v%u.%02u...", r.versionInt / 100, r.versionInt % 100);
  lv_refr_now(nullptr);
  OTA::perform(r.url, [](int p) {             // blocking; restarts on success
    lv_label_set_text_fmt(s_msgLbl, "Downloading... %d%%", p);
    lv_refr_now(nullptr);
  });
  lv_label_set_text_fmt(s_msgLbl, "Update failed: %s", OTA::performError());
}

static lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb,
                            lv_align_t align, int xofs, int yofs) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, 220, 70);
  lv_obj_align(btn, align, xofs, yofs);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  return btn;
}

static void buildStatusScreen() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "ANBRU-430  fw v" FIRMWARE_VERSION);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  s_wifiLbl = lv_label_create(scr);
  lv_label_set_text(s_wifiLbl, "WiFi: ...");
  lv_obj_set_style_text_color(s_wifiLbl, lv_color_hex(0xE0E4E8), 0);
  lv_obj_align(s_wifiLbl, LV_ALIGN_TOP_LEFT, 20, 60);

  s_dcsLbl = lv_label_create(scr);
  lv_label_set_text(s_dcsLbl, "DCS: waiting");
  lv_obj_set_style_text_color(s_dcsLbl, lv_color_hex(0xE0E4E8), 0);
  lv_obj_align(s_dcsLbl, LV_ALIGN_TOP_LEFT, 20, 90);

  s_fuelLbl = lv_label_create(scr);
  lv_label_set_text(s_fuelLbl, "FUEL: ---");
  lv_obj_set_style_text_color(s_fuelLbl, lv_color_hex(0xE0E4E8), 0);
  lv_obj_align(s_fuelLbl, LV_ALIGN_TOP_LEFT, 20, 120);

  s_msgLbl = lv_label_create(scr);
  lv_label_set_text(s_msgLbl, "");
  lv_obj_set_style_text_color(s_msgLbl, lv_color_hex(0xFFC040), 0);
  lv_obj_align(s_msgLbl, LV_ALIGN_BOTTOM_MID, 0, -110);

  s_heapLbl = lv_label_create(scr);
  lv_label_set_text(s_heapLbl, "heap ---");
  lv_obj_set_style_text_color(s_heapLbl, lv_color_hex(0x707880), 0);
  lv_obj_align(s_heapLbl, LV_ALIGN_TOP_RIGHT, -12, 10);

  makeButton(scr, "Check Update", onCheckUpdate, LV_ALIGN_BOTTOM_LEFT, 40, -20);
  makeButton(scr, "USB Flash",    onUsbFlash,    LV_ALIGN_BOTTOM_RIGHT, -40, -20);
}

void setup() {
  Serial.begin(115200);   // no CDC on this board once USB HID is up; harmless

  loadPrefs();

  if (!Display::begin() || !LvglPort::begin()) return;
  buildStatusScreen();
  Touch::begin();

  // First boot has no WiFi credentials → BLE terminal setup (same flow as Brew370).
  // HID::begin (USB) deliberately comes AFTER this block: TinyUSB active during
  // Bluedroid advertising kills the advertisement on IDF 5.5.4 (bench-diagnosed
  // 2026-07-14: DEV unit w/o USB advertises, ANBRU w/ USB does not).
  if (!WifiMgr::hasCredentials()) {
    msg("No WiFi. Connect BLE terminal to ANBRU-430 to set credentials.");
    // Diagnostic readout: BLE controller status (0=off 1=inited 2=enabled),
    // free internal heap, client-subscribed flag — refreshed once per second.
    static auto bleDiagCb = []() {
      static unsigned long last = 0;
      if (millis() - last >= 1000) {
        last = millis();
        lv_label_set_text_fmt(s_msgLbl, "BLE wait: ctrl=%d heap=%u sub=%d",
                              (int)esp_bt_controller_get_status(),
                              (unsigned)ESP.getFreeHeap(),
                              (int)WifiMgr::isBleClientConnected());
        lv_refr_now(nullptr);
      }
    };
    if (WifiMgr::runBleSetup(bleDiagCb, []() { return false; })) {
      msg("Saved. Rebooting...");
      delay(800);
      ESP.restart();
    }
  }

  HID::begin(s_screenW, s_screenH);   // USB identity from anbru430/config.h

  msg("Connecting WiFi...");
  WifiMgr::beginConnect(true);

#ifdef DEV_BUILD
  {
    static auto modeIdFn   = []() -> uint8_t { return 0; };
    static auto modeNameFn = []() -> const char* { return "SPIKE"; };
    static auto menuSelFn  = []() -> int { return 0; };
    static auto wifiBootFn = []() { WifiMgr::beginConnect(true); };
    static auto injectFn   = [](const char* verb, const char* rest) -> bool {
      if (strcmp(verb, "touch") != 0) return false;
      int x = -1, y = -1;
      if (sscanf(rest, "%d %d", &x, &y) != 2) return false;
      if (x < 0 || x >= 800 || y < 0 || y >= 480) return false;
      Touch::inject((uint16_t)x, (uint16_t)y);
      return true;
    };
    Shell::begin(Shell::Hooks{ modeIdFn, modeNameFn, menuSelFn, wifiBootFn,
                               injectFn, nullptr });
  }
#endif
}

void loop() {
  Shell::poll();
  LvglPort::loop();
  DcsBios::process();

  // WiFi tracker — start DCS-BIOS on each connect transition (oled main pattern)
  static bool s_wasConnected = false;
  static unsigned long s_lastRefresh = 0;
  bool wifiOk = WifiMgr::isConnected();
  if (wifiOk && !s_wasConnected) {
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   nullptr, DCSBIOS_CMD_PORT);
    msg("");
  }
  s_wasConnected = wifiOk;

  if (millis() - s_lastRefresh >= 500) {
    s_lastRefresh += 500;
    if (wifiOk) {
      lv_label_set_text_fmt(s_wifiLbl, "WiFi: %s  %s",
                            WifiMgr::activeSSID(), WiFi.localIP().toString().c_str());
    } else {
      lv_label_set_text(s_wifiLbl, "WiFi: connecting...");
    }
    lv_label_set_text(s_dcsLbl, DcsBios::isConnected() ? "DCS: connected" : "DCS: waiting");
    if (DcsBios::isConnected())
      lv_label_set_text_fmt(s_fuelLbl, "FUEL: %u lbs", (unsigned)DcsBios::fuelLbs());
    lv_label_set_text_fmt(s_heapLbl, "heap %u", (unsigned)ESP.getFreeHeap());
  }
}
