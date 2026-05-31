#include <Arduino.h>
#include <Preferences.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"
#include "hardware.h"
#include "encoder.h"
#include "hid.h"
#include "macros.h"

enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, MOUSE_TUNE_EDIT, WIFI_MENU
};

static MenuState s_mode           = MACRO_MENU;
static int  s_currentMacro        = 0;
static int  s_menuSel             = 0;
static int  s_menuOffset          = 0;
static int  s_handedness          = 0;
static int  s_brightness          = 20;
static int  s_prevBrightness      = 20;
static int  s_sleepSecs           = 45;
static int  s_prevSleepSecs       = 45;
static int  s_mouseTuneSel        = 0;
static int  s_mouseTuneOffset     = 0;
static int  s_editParamIdx        = 0;
static int  s_editDigits[4]       = {0,0,0,0};
static int  s_editDigitPos        = 0;
static int  s_prevMouseParams[6];
static int  s_wifiSubSel          = 0;
static bool s_mcActive            = false;
static bool s_mcFlash             = false;
static unsigned long s_mcFlashTimer = 0;
static bool s_wasDcsConnected     = false;
static bool s_syncDone            = false;
static unsigned long s_lastOled   = 0;
static bool s_oledSleeping        = false;
static unsigned long s_lastActivity = 0;

static void loadNvs() {
  Preferences prefs;
  prefs.begin("brew", true);
  s_brightness  = prefs.getInt("brightness", 20);
  s_handedness  = prefs.getInt("hand", 0);
  s_sleepSecs   = prefs.getInt("sleep", 45);
  mouseParams[0] = prefs.getInt("ptX", 460);
  mouseParams[1] = prefs.getInt("ptY", 8);
  mouseParams[2] = prefs.getInt("mcX", 555);
  mouseParams[3] = prefs.getInt("mcY", 295);
  mouseParams[4] = prefs.getInt("lbX", 10);
  mouseParams[5] = prefs.getInt("lbY", 26);
  prefs.end();
  memcpy(s_prevMouseParams, mouseParams, sizeof(mouseParams));
}

static void executeMenuItem() {
  switch (s_menuSel) {
    case 0:  // Reboot
      ESP.restart();
      break;
    case 1:  // Hand
      s_handedness = 1 - s_handedness;
      { Preferences p; p.begin("brew", false); p.putInt("hand", s_handedness); p.end(); }
      return;
    case 2:  // Brightness
      s_prevBrightness = s_brightness;
      s_mode = BRIGHTNESS_ADJUST;
      return;
    case 3:  // Sleep
      s_prevSleepSecs = s_sleepSecs;
      s_mode = SLEEP_ADJUST;
      return;
    case 4:  // Mouse Tune
      memcpy(s_prevMouseParams, mouseParams, sizeof(mouseParams));
      s_mouseTuneSel = 0; s_mouseTuneOffset = 0;
      s_mode = MOUSE_TUNE_MENU;
      return;
    case 5:  // WiFi — placeholder, wired in Tasks 9+10
      s_mode = WIFI_MENU;
      return;
    case 6:  // EXIT
      s_mode = MACRO_MENU;
      return;
  }
  s_menuSel = 0; s_menuOffset = 0;
}

static void executeMouseTuneItem() {
  if (s_mouseTuneSel < 6) {
    s_editParamIdx = s_mouseTuneSel;
    int v = mouseParams[s_editParamIdx];
    s_editDigits[0] = v / 1000;
    s_editDigits[1] = (v / 100) % 10;
    s_editDigits[2] = (v / 10) % 10;
    s_editDigits[3] = v % 10;
    s_editDigitPos = 0;
    s_mode = MOUSE_TUNE_EDIT;
    return;
  }
  if (s_mouseTuneSel == 8) {  // Save+Exit
    Preferences p; p.begin("brew", false);
    p.putInt("ptX", mouseParams[0]); p.putInt("ptY", mouseParams[1]);
    p.putInt("mcX", mouseParams[2]); p.putInt("mcY", mouseParams[3]);
    p.putInt("lbX", mouseParams[4]); p.putInt("lbY", mouseParams[5]);
    p.end();
    UI::showSaved();
    s_mode = SETTINGS;
  } else if (s_mouseTuneSel == 9) {  // Cancel
    memcpy(mouseParams, s_prevMouseParams, sizeof(mouseParams));
    s_mode = SETTINGS;
  } else if (s_mouseTuneSel == 6) {  // Run:Pin (test)
    if (HID::isReady()) {
      HID::homeMouse();
      HID::moveMouseTotal(mouseParams[0], mouseParams[1]);
      HID::mouseClick();
    }
  } else if (s_mouseTuneSel == 7) {  // Run:Ctr (test)
    if (HID::isReady()) {
      HID::homeMouse();
      HID::moveMouseTotal(mouseParams[0], mouseParams[1]);
      HID::mouseClick();
      HID::moveMouseTotal(mouseParams[2], mouseParams[3]);
    }
  }
  s_mouseTuneSel = 0; s_mouseTuneOffset = 0;
}

void setup() {
#ifndef RELEASE_BUILD
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);
#endif

  loadNvs();
  HID::begin();
  UI::begin();

  Encoder::begin();  // init early so splash can be dismissed

  UI::setContrast(s_brightness);
  UI::showSplash();

  // Hold splash up to 30 s; any encoder input dismisses it early
  {
    unsigned long start = millis();
    while (millis() - start < 30000UL) {
      int8_t d = Encoder::readDelta();
      if (d != 0 || Encoder::shortPressed() || Encoder::longPressed()) break;
      delay(10);
    }
    Encoder::flush();
  }

  UI::showWifiConnecting(WIFI_SSID_DEFAULT);

  bool wifiOk = WifiMgr::begin();
  if (wifiOk) {
#ifndef RELEASE_BUILD
    Serial.printf("WiFi connected: %s\n", WifiMgr::activeSSID());
#endif
    UI::showWifiConnected(WifiMgr::activeSSID());
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   "255.255.255.255", DCSBIOS_CMD_PORT);
    UI::showSyncing();
  } else {
#ifndef RELEASE_BUILD
    Serial.printf("WiFi failed: %s\n", WifiMgr::activeSSID());
#endif
    UI::showWifiFailed(WifiMgr::activeSSID());
  }

  Hardware::begin();
  s_lastActivity = millis();
}

void loop() {
  bool dcsActivity = DcsBios::update();
  bool mc          = DcsBios::masterCaution();

  int8_t delta = Encoder::readDelta();
  bool encActivity = (delta != 0) || Encoder::shortPressed() || Encoder::longPressed();

  // Wake OLED on any activity
  if (s_oledSleeping && encActivity) {
    UI::wake();
    s_oledSleeping = false;
    s_lastActivity = millis();
  }
  if (!s_oledSleeping && (encActivity || dcsActivity)) {
    s_lastActivity = millis();
  }

  // MASTER CAUTION — debounce 200ms to reject large-block export transients
  static unsigned long s_mcHighSince = 0;
  if (mc && s_mcHighSince == 0)  s_mcHighSince = millis();
  if (!mc)                        s_mcHighSince = 0;
  bool mcConfirmed = mc && (millis() - s_mcHighSince >= 200);

  if (mcConfirmed && s_oledSleeping) {
    UI::wake();
    s_oledSleeping = false;
    s_lastActivity = millis();
  }
  if (mcConfirmed) {
    s_mcActive = true;
    if (millis() - s_mcFlashTimer > 200) {
      s_mcFlash = !s_mcFlash;
      s_mcFlashTimer = millis();
    }
    UI::showMasterCaution(s_mcFlash);
    if (Encoder::shortPressed()) {
      DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 1);
      delay(100);
      DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 0);
    }
    return;
  }
  if (s_mcActive && !mcConfirmed) s_mcActive = false;

  // Normal operation
  Hardware::update();

  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !s_wasDcsConnected) {
    UI::showSyncing(); delay(800); UI::showSynced();
    s_syncDone = true;
  }
  if (!nowConnected && s_syncDone) {
    s_syncDone = false;
    UI::showSyncFailed();
  }
  s_wasDcsConnected = nowConnected;

  // Menu state machine
  if (s_mode == MACRO_MENU) {
    s_currentMacro = (s_currentMacro + delta + numMacros) % numMacros;
    if (Encoder::shortPressed()) { UI::flashScreen(); executeMacro(s_currentMacro); }
    if (Encoder::longPressed()) {
      s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0;
      UI::flashScreen();
    }

  } else if (s_mode == SETTINGS) {
    s_menuSel = (s_menuSel + delta + 7) % 7;
    if (s_menuSel < s_menuOffset) s_menuOffset = s_menuSel;
    if (s_menuSel >= s_menuOffset + 4) s_menuOffset = s_menuSel - 3;
    if (Encoder::shortPressed()) executeMenuItem();

  } else if (s_mode == BRIGHTNESS_ADJUST) {
    s_brightness = constrain(s_brightness + delta * 10, 5, 255);
    UI::setContrast(s_brightness);
    if (Encoder::shortPressed()) {
      Preferences p; p.begin("brew", false); p.putInt("brightness", s_brightness); p.end();
      s_mode = SETTINGS;
    }
    if (Encoder::longPressed()) {
      s_brightness = s_prevBrightness;
      UI::setContrast(s_brightness);
      s_mode = SETTINGS;
    }

  } else if (s_mode == SLEEP_ADJUST) {
    if (delta > 0) {
      if (s_sleepSecs > 0 && s_sleepSecs < 120) s_sleepSecs += 5;
      else if (s_sleepSecs == 120) s_sleepSecs = 0;
    } else if (delta < 0) {
      if (s_sleepSecs == 0) s_sleepSecs = 120;
      else if (s_sleepSecs > 10) s_sleepSecs -= 5;
    }
    if (Encoder::shortPressed()) {
      Preferences p; p.begin("brew", false); p.putInt("sleep", s_sleepSecs); p.end();
      s_mode = SETTINGS;
    }
    if (Encoder::longPressed()) { s_sleepSecs = s_prevSleepSecs; s_mode = SETTINGS; }

  } else if (s_mode == MOUSE_TUNE_MENU) {
    s_mouseTuneSel = (s_mouseTuneSel + delta + 10) % 10;
    if (s_mouseTuneSel < s_mouseTuneOffset) s_mouseTuneOffset = s_mouseTuneSel;
    if (s_mouseTuneSel >= s_mouseTuneOffset + 3) s_mouseTuneOffset = s_mouseTuneSel - 2;
    if (Encoder::shortPressed()) { UI::flashScreen(); executeMouseTuneItem(); }
    if (Encoder::longPressed()) {
      memcpy(mouseParams, s_prevMouseParams, sizeof(mouseParams));
      s_mode = SETTINGS; UI::flashScreen();
    }

  } else if (s_mode == MOUSE_TUNE_EDIT) {
    s_editDigits[s_editDigitPos] = (s_editDigits[s_editDigitPos] + delta + 10) % 10;
    if (Encoder::shortPressed()) {
      if (s_editDigitPos < 3) {
        s_editDigitPos++;
      } else {
        mouseParams[s_editParamIdx] =
          s_editDigits[0]*1000 + s_editDigits[1]*100 +
          s_editDigits[2]*10  + s_editDigits[3];
        s_mode = MOUSE_TUNE_MENU;
        UI::flashScreen();
      }
    }
    if (Encoder::longPressed()) { s_mode = MOUSE_TUNE_MENU; UI::flashScreen(); }

  } else if (s_mode == WIFI_MENU) {
    s_wifiSubSel = (s_wifiSubSel + delta + 3) % 3;
    if (Encoder::shortPressed()) {
      if (s_wifiSubSel == 0) {
        // Serial Entry
        bool saved = WifiMgr::runSerialSetup(
          []() { UI::showSerialActive(); },
          []() { return Encoder::longPressed(); }
        );
        if (saved) UI::showSaved();
        s_mode = SETTINGS;
      } else if (s_wifiSubSel == 1) {
        // Manual Entry — encoder character scroll
        char newSSID[33] = {0};
        char newPass[64] = {0};
        bool gotSSID = WifiMgr::runEncoderEntry(
          "SSID", newSSID, sizeof(newSSID),
          []() { return Encoder::readDelta(); },
          []() { return Encoder::shortPressed(); },
          []() { return Encoder::longPressed(); },
          [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
        );
        if (gotSSID) {
          bool gotPass = WifiMgr::runEncoderEntry(
            "Password", newPass, sizeof(newPass),
            []() { return Encoder::readDelta(); },
            []() { return Encoder::shortPressed(); },
            []() { return Encoder::longPressed(); },
            [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
          );
          if (gotPass) {
            WifiMgr::saveCredentials(newSSID, newPass);
            UI::showSaved();
          }
        }
        s_mode = SETTINGS;
      } else {
        // Back
        s_mode = SETTINGS;
      }
      s_wifiSubSel = 0;
    }
    if (Encoder::longPressed()) { s_wifiSubSel = 0; s_mode = SETTINGS; }
  }

  // OLED sleep check
  if (!s_oledSleeping && s_sleepSecs > 0 &&
      millis() - s_lastActivity > (unsigned long)s_sleepSecs * 1000UL) {
    UI::sleep();
    s_oledSleeping = true;
  }

  // OLED update — skip while display is powered down
  if (!s_oledSleeping && millis() - s_lastOled > 200) {
    s_lastOled = millis();
    switch (s_mode) {
      case MACRO_MENU:        UI::showMacroMenu(s_currentMacro); break;
      case SETTINGS:          UI::showSettingsMenu(s_menuSel, s_menuOffset,
                                s_handedness, WifiMgr::isConnected(),
                                DcsBios::isConnected()); break;
      case BRIGHTNESS_ADJUST: UI::showBrightnessAdjust(s_brightness); break;
      case SLEEP_ADJUST:      UI::showSleepAdjust(s_sleepSecs); break;
      case MOUSE_TUNE_MENU:   UI::showMouseTuneMenu(s_mouseTuneSel, s_mouseTuneOffset); break;
      case MOUSE_TUNE_EDIT:   UI::showMouseTuneEdit(s_editParamIdx,
                                s_editDigits, s_editDigitPos); break;
      case WIFI_MENU:         UI::showWifiSubMenu(s_wifiSubSel); break;
    }
  }
}
