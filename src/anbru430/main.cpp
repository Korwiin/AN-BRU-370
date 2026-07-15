#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>
#include "esp32-hal-tinyusb.h"  // usb_persist_restart (settings_update.cpp uses it too)
#include "config.h"
#include "display.h"
#include "lvgl_port.h"
#include "touch.h"
#include "ui.h"
#include "wifi_mgr.h"
#include "dcs_bios.h"
#include "hid.h"
#include "macros.h"
#include "ota.h"
#include "shell.h"

// ---- App state ----
enum AppState : uint8_t {
  ST_NO_CREDS, ST_WIFI_CONNECTING, ST_WIFI_FAILED,
  ST_WAITING_DCS, ST_AIRCRAFT, ST_NOT_READY, ST_SETUP_RUNNING
};
static AppState s_state = ST_WIFI_CONNECTING;

static const char* appStateName(AppState s) {
  static const char* k_names[] = {
    "NO_CREDS", "WIFI_CONNECTING", "WIFI_FAILED",
    "WAITING_DCS", "AIRCRAFT", "NOT_READY", "SETUP_RUNNING"
  };
  return ((unsigned)s < sizeof(k_names)/sizeof(k_names[0])) ? k_names[s] : "?";
}

// ---- Prefs (same NVS keys as Brew370 where semantics match) ----
static int s_screenW  = 1920;
static int s_screenH  = 1080;
static int s_sleepSecs = 45;
static int s_dimLevel  = 0;

static void loadPrefs() {
  Preferences prefs;
  prefs.begin("brew", true);
  s_screenW  = prefs.getInt("scrW", 1920);
  s_screenH  = prefs.getInt("scrH", 1080);
  s_sleepSecs = prefs.getInt("sleep", 45);
  s_dimLevel  = prefs.getInt("dimlvl", 0);
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

// ---- Sleep (backlight off; any touch wakes, wake touch swallowed) ----
static unsigned long s_lastActivity = 0;
static bool s_sleeping = false;

static void wake() {
  Display::setBacklight(true);
  Touch::swallowUntilRelease();   // if no finger is down, next empty scan clears it
  s_sleeping = false;
  s_lastActivity = millis();
}

// ---- WiFi connect flow (non-blocking; loop-driven) ----
static unsigned long s_wifiDeadline = 0;

static void enterState(AppState st) {
  s_state = st;
  switch (st) {
    case ST_NO_CREDS:        PageHome::setState(PageHome::NO_CREDS); break;
    case ST_WIFI_CONNECTING: PageHome::setSsid(WifiMgr::activeSSID());
                             PageHome::setState(PageHome::WIFI_CONNECTING); break;
    case ST_WIFI_FAILED:     PageHome::setState(PageHome::WIFI_FAILED); break;
    case ST_WAITING_DCS:     PageHome::setState(PageHome::WAITING_DCS); break;
    case ST_AIRCRAFT:        PageHome::setState(PageHome::AIRCRAFT); break;
    case ST_NOT_READY:                                 // alert modal carries the visuals;
    case ST_SETUP_RUNNING:   PageHome::setState(PageHome::AIRCRAFT); break;
  }
}

static void startConnect(bool full) {
  if (WifiMgr::beginConnect(full)) {
    s_wifiDeadline = millis() + 65000UL;
    enterState(ST_WIFI_CONNECTING);
  } else {
    enterState(ST_NO_CREDS);
  }
}

// ---- BLE credential setup (MUST run before HID::begin — TinyUSB kills
// Bluedroid advertising on IDF 5.5.4). Runtime entry = NVS flag + reboot. ----
static volatile bool s_bleCancel = false;
static lv_obj_t*     s_bleStatusLbl = nullptr;

static bool consumeBleRequestFlag() {
  Preferences p;
  p.begin("brew", false);
  bool req = p.getInt("blereq", 0) != 0;
  if (req) p.remove("blereq");
  p.end();
  return req;
}

static void bleSetupSession() {
  s_bleCancel = false;
  lv_obj_t* m = lv_obj_create(lv_layer_top());
  lv_obj_set_size(m, 800, 480);
  lv_obj_set_pos(m, 0, 0);
  UI::stripPanel(m);
  lv_obj_t* t = UI::makeLabel(m, "BLE CREDENTIAL SETUP", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_t* i = UI::makeLabel(m,
      "Connect a BLE terminal to \"" BLE_DEVICE_NAME "\"\n(Nordic UART) and follow the prompts.",
      &lv_font_montserrat_20, UI::colText());
  lv_obj_set_style_text_align(i, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(i, LV_ALIGN_CENTER, 0, -50);
  s_bleStatusLbl = UI::makeLabel(m, "Client: waiting", &lv_font_montserrat_20, UI::colWarn());
  lv_obj_align(s_bleStatusLbl, LV_ALIGN_CENTER, 0, 20);
  lv_obj_t* c = UI::makeButton(m, "Cancel",
      [](lv_event_t*) { s_bleCancel = true; }, nullptr);
  lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_refr_now(nullptr);

  bool saved = WifiMgr::runBleSetup(
      []() {
        static unsigned long last = 0;
        if (millis() - last >= 500) {
          last = millis();
          lv_label_set_text_fmt(s_bleStatusLbl, "Client: %s",
                                WifiMgr::isBleClientConnected() ? "connected" : "waiting");
        }
        LvglPort::loop();   // keep render + touch alive so Cancel works
      },
      []() -> bool { return s_bleCancel; });

  if (saved) {
    lv_label_set_text(s_bleStatusLbl, "Saved. Rebooting...");
    lv_refr_now(nullptr);
    delay(800);
    ESP.restart();
  }
  lv_obj_delete(m);
  s_bleStatusLbl = nullptr;
}

void setup() {
  Serial.begin(115200);
#ifdef DEV_BUILD
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== ANBRU-430 v%s boot (dev) ===\n", FIRMWARE_VERSION);
#endif

  loadPrefs();
  if (!Display::begin() || !LvglPort::begin()) return;
  UI::begin();
  Touch::begin();
  UI::setDimLevel(s_dimLevel);

  PageHome::setRetryCb([]() { startConnect(true); });
  PageHome::setBleSetupCb([]() { ESP.restart(); });  // no creds → boot re-enters BLE setup
  PageSettings::setDimPtr(&s_dimLevel);
  PageSettings::setSleepSecsPtr(&s_sleepSecs);
  SettingsMouse::setScreenPtrs(&s_screenW, &s_screenH);
  SettingsWifi::setConnectCb([]() {
    startConnect(false);
    UI::showPage(UI::PAGE_HOME);
  });
  SettingsWifi::setBleRequestCb([]() {
    Preferences p;
    p.begin("brew", false);
    p.putInt("blereq", 1);
    p.end();
    ESP.restart();
  });

#ifdef DEV_BUILD
  {
    static auto modeIdFn   = []() -> uint8_t { return (uint8_t)s_state; };
    static auto modeNameFn = []() -> const char* { return appStateName(s_state); };
    static auto menuSelFn  = []() -> int { return (int)UI::currentPage(); };
    static auto wifiBootFn = []() { startConnect(true); };  // non-blocking on this device
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

  bool bleReq = consumeBleRequestFlag();
  if (!WifiMgr::hasCredentials() || bleReq) bleSetupSession();  // restarts if saved

  HID::begin(s_screenW, s_screenH);
  startConnect(true);
  s_lastActivity = millis();
}

void loop() {
  Shell::poll();
  LvglPort::loop();

  // WiFi tracker — (re)start DCS-BIOS on every up-transition
  {
    static bool s_wasUp = false;
    bool up = WifiMgr::isConnected();
    if (up && !s_wasUp)
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT, nullptr, DCSBIOS_CMD_PORT);
    s_wasUp = up;
  }

  // Connect-flow transitions
  if (s_state == ST_WIFI_CONNECTING) {
    if (WifiMgr::isConnected()) enterState(ST_WAITING_DCS);
    else if (millis() >= s_wifiDeadline) enterState(ST_WIFI_FAILED);
  } else if (s_state == ST_WIFI_FAILED) {
    if (WifiMgr::isConnected()) enterState(ST_WAITING_DCS);  // auto-reconnect landed
  }

  bool dcsActivity = DcsBios::process();

  // Activity + sleep
  {
    static uint32_t s_prevTouchMs = 0;
    uint32_t tMs = Touch::lastTouchMs();
    bool touchActivity = (tMs != s_prevTouchMs);
    s_prevTouchMs = tMs;
    if (s_sleeping && touchActivity) { wake(); return; }
    if (!s_sleeping && (touchActivity || dcsActivity)) s_lastActivity = millis();
  }

  // TASK4_ALERTS_INSERT — RWR missile + Master Caution / Stores Config

  // DCS connection transitions
  {
    static bool s_wasDcs = false;
    bool nowDcs = DcsBios::isConnected();
    if (nowDcs && !s_wasDcs) {
      if (s_state == ST_WAITING_DCS) enterState(ST_AIRCRAFT);
    }
    if (!nowDcs && s_wasDcs) {
      if (s_state == ST_AIRCRAFT || s_state == ST_NOT_READY || s_state == ST_SETUP_RUNNING) {
        enterState(ST_WAITING_DCS);
        Alerts::hide();
      }
    }
    s_wasDcs = nowDcs;
  }

  // TASK5_SETUP_INSERT — NOT READY + setup sequence

  // Periodic page refresh
  {
    static unsigned long s_lastRefresh = 0;
    if (millis() - s_lastRefresh >= 500) {
      s_lastRefresh = millis();
      if (s_state == ST_AIRCRAFT) PageHome::setFuel(DcsBios::fuelLbs());
      if (UI::currentPage() == UI::PAGE_STATUS) PageStatus::refresh();
    }
  }

  // Sleep check
  if (!s_sleeping && s_sleepSecs > 0 &&
      millis() - s_lastActivity > (unsigned long)s_sleepSecs * 1000UL) {
    Display::setBacklight(false);
    s_sleeping = true;
  }
}
