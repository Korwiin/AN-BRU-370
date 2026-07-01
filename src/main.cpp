#include <Arduino.h>
#include <Preferences.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"
#include "encoder.h"
#include "hid.h"
#include "macros.h"
#include "ota.h"
#include <WiFi.h>

enum MenuState {
  BOOT_STATUS,
  AIRCRAFT_STATUS,
  NOT_READY,       // MWS=OFF: new-plane alert
  SETUP_RUNNING,   // executing 5-step setup sequence
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, WIFI_MENU, SECRETS_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT,
  FIRMWARE_CHECKING, FIRMWARE_UP_TO_DATE,
  FIRMWARE_CONFIRM, FIRMWARE_UPDATING, FIRMWARE_ERROR
};

enum ScState : uint8_t {
  SC_IDLE, SC_WAITING_SW, SC_WAITING_LIGHT, SC_GAVE_UP
};

static MenuState s_mode           = BOOT_STATUS;
static int  s_currentMacro        = 0;
static unsigned long s_macroLastInput = 0;
static int  s_menuSel             = 0;
static int  s_menuOffset          = 0;
static bool s_encReversed         = true;
static int  s_brightness          = 20;
static int  s_prevBrightness      = 20;
static int  s_sleepSecs           = 45;
static int  s_prevSleepSecs       = 45;
static int  s_mouseTuneSel        = 0;
static int  s_mouseTuneOffset     = 0;
static int     s_wifiSubSel       = 0;
static uint8_t s_gStatus          = 0;   // 0=pending, 1=online, 2=offline
static bool    s_gChecked         = false;
static char    s_nvsSsid[64]      = {0};
static uint8_t s_nvsPassStatus    = 0;
static bool s_mcActive            = false;
static bool s_mcFlash             = false;
static unsigned long s_mcFlashTimer = 0;
static bool          s_rwrActive     = false;
static bool          s_rwrFlash      = false;
static unsigned long s_rwrFlashTimer = 0;
static bool s_wasDcsConnected     = false;
static bool          s_wasMwsOn   = false;
static uint8_t       s_setupStep  = 0;    // 0=idle, 1–5=current step
static unsigned long s_setupSent  = 0;    // millis() of last command send
static unsigned long s_setupStart = 0;    // millis() of step entry (for timeout)
static unsigned long s_lastOled   = 0;
static bool s_oledSleeping        = false;
static unsigned long s_lastActivity = 0;
static bool s_dcsBiosStarted = false;

static bool          s_bootStarted  = false;
static unsigned long s_bootDoneAt   = 0;   // millis() when ph.ip && dcsLive first both true
static unsigned long s_bootRetryAt  = 0;   // 0 = no retry pending; else millis() deadline for next startWifi()

static OTA::CheckResult  s_otaResult        = {};
static char              s_otaError[24]      = {0};
static bool              s_otaPerformFailed  = false;

static ScState       s_scState     = SC_IDLE;
static uint8_t       s_scTarget    = 0xFF;
static unsigned long s_scTPress    = 0;
static unsigned long s_scTLastSend = 0;

static int           s_calibIdx        = 0;
static uint16_t      s_calibX          = 0;
static uint16_t      s_calibY          = 0;
static unsigned long s_lastCalibTick   = 0;
static int s_screenW         = 1920;
static int s_screenH         = 1080;
static int s_screenDigits[8] = {0};
static int s_screenDigitPos  = 0;

static MenuState homeMode();
static bool inOtaMode() { return s_mode >= FIRMWARE_CHECKING; }

static void loadNvs() {
  Preferences prefs;
  prefs.begin("brew", true);
  s_brightness  = prefs.getInt("brightness", 20);
  s_encReversed = prefs.getInt("encrev", 1);
  s_sleepSecs   = prefs.getInt("sleep", 45);
  // Screen dims loaded first — calibration defaults are proportional to them
  s_screenW = prefs.getInt("scrW", 1920);
  s_screenH = prefs.getInt("scrH", 1080);
  // TODO: remove stale NVS keys aptX/aptY/amcX/amcY (0-4095 space, now abandoned)
  mouseParams[0] = prefs.getInt("apxX",  s_screenW / 4);
  mouseParams[1] = prefs.getInt("apxY",  s_screenH / 54);
  mouseParams[2] = prefs.getInt("amcX2", s_screenW / 2);
  mouseParams[3] = prefs.getInt("amcY2", s_screenH / 2);
  mouseParams[4] = prefs.getInt("lbX2", s_screenW / 2);
  mouseParams[5] = prefs.getInt("lbY2", s_screenH / 2);
  mouseParams[6] = prefs.getInt("cdrpX", s_screenW / 5);
  mouseParams[7] = prefs.getInt("cdrpY", s_screenH / 2);
  prefs.end();
}

static void otaProgressCb(int percent) {
  UI::showFirmwareUpdating(percent);
}

static void safeRestart() {
  ESP.restart();
}

static void rebootWithCountdown() {
  for (int secs = 5; secs > 0; secs--) {
    UI::showRebootCountdown(secs);
    unsigned long t0 = millis();
    while (millis() - t0 < 1000UL) {
      Encoder::readDelta();
      if (Encoder::shortPressed()) return;  // SP = cancel
      delay(50);
    }
  }
  safeRestart();
}

static void executeMenuItem() {
  switch (s_menuSel) {
    case 0:  // Knob direction
      s_encReversed = !s_encReversed;
      { Preferences p; p.begin("brew", false); p.putInt("encrev", s_encReversed); p.end(); }
      return;
    case 1:  // Brightness
      s_prevBrightness = s_brightness;
      s_mode = BRIGHTNESS_ADJUST;
      return;
    case 2:  // Sleep
      s_prevSleepSecs = s_sleepSecs;
      s_mode = SLEEP_ADJUST;
      return;
    case 3:  // WiFi
      s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
      s_mode = WIFI_MENU;
      return;
    case 4:  // Mouse Tune
      s_mouseTuneSel = 0; s_mouseTuneOffset = 0;
      s_mode = MOUSE_TUNE_MENU;
      return;
    case 5:  // Firmware
      s_mode = FIRMWARE_CHECKING;
      return;
    case 6:  // Reboot
      rebootWithCountdown();
      break;
    case 7:  // EXIT
      s_mode = homeMode();
      return;
  }
  s_menuSel = 0; s_menuOffset = 0;
}

static void executeMouseTuneItem() {
  if (s_mouseTuneSel == 0) {
    s_screenDigits[0] = s_screenW / 1000;
    s_screenDigits[1] = (s_screenW / 100) % 10;
    s_screenDigits[2] = (s_screenW / 10)  % 10;
    s_screenDigits[3] = s_screenW % 10;
    s_screenDigits[4] = s_screenH / 1000;
    s_screenDigits[5] = (s_screenH / 100) % 10;
    s_screenDigits[6] = (s_screenH / 10)  % 10;
    s_screenDigits[7] = s_screenH % 10;
    s_screenDigitPos  = 0;
    s_mode = SCREEN_EDIT;
    return;
  }
  if (s_mouseTuneSel >= 1 && s_mouseTuneSel <= 4) {
    int ci = s_mouseTuneSel - 1;  // calibIdx: 0=PinTool 1=MapCtr 2=PinLabel 3=ClickOut
    if (ci == 2) {
      // Pin Label POS: drop a pin to open label dialog before calibrating
      HID::Keyboard.releaseAll();
      HID::pressKey(KEY_F10);
      delay(30);
      HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
      HID::mouseClick();
      HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
      HID::mouseClick();
      delay(400);
    }
    s_calibIdx      = ci;
    s_calibX        = (uint16_t)mouseParams[ci * 2];
    s_calibY        = (uint16_t)mouseParams[ci * 2 + 1];
    s_lastCalibTick = millis();
    HID::moveAbs(s_calibX, s_calibY);
    s_mode = MOUSE_CALIBRATE_X;
    return;
  }
  // sel == 5: Back
  s_mode = SETTINGS;
}

static const char* kCalibLabelX[] = {
  "Move L/R to Map Label",
  "Move L/R to MapCenter",
  "Move L/R to TextField",
  "Move L/R to Click Out"
};
static const char* kCalibLabelY[] = {
  "Move U/D to Map Label",
  "Move U/D to MapCenter",
  "Move U/D to TextField",
  "Move U/D to Click Out"
};

static MenuState homeMode() {
  if (!DcsBios::isConnected()) return BOOT_STATUS;
  return DcsBios::mwsOn() ? AIRCRAFT_STATUS : NOT_READY;
}

void setup() {
#ifndef RELEASE_BUILD
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);
#endif

  loadNvs();
  HID::begin(s_screenW, s_screenH);
  UI::begin();

  Encoder::begin();  // init early so splash can be dismissed

  UI::setContrast(s_brightness);

  // USB settle — PMU shared between USB-OTG and WiFi; don't start WiFi until
  // OTG enumeration completes (~3s). Show boot status with all -- during wait.
  {
    unsigned long settleStart = millis();
    BootStatusInfo bsi = {};  // all false/zero
    while (millis() - settleStart < 3000UL) {
      UI::showBootStatus(bsi);
      Encoder::readDelta();
      delay(10);
    }
    Encoder::flush();
  }

  // Check for empty WiFi credentials on first boot
  if (!WifiMgr::hasCredentials()) {
    UI::showNoCredentials();
    while (!Encoder::shortPressed() && !Encoder::longPressed()) {
      Encoder::readDelta();
      delay(10);
    }
    s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
    s_mode = WIFI_MENU;
    s_lastActivity = millis();
    return;
  }

  // WiFi startup handled by BOOT_STATUS state in loop().
  s_lastActivity = millis();
}

void loop() {
  // Background DCS-BIOS start for post-boot reconnects (boot path uses BOOT_STATUS state)
  if (s_mode != BOOT_STATUS && !s_dcsBiosStarted) {
    if (WifiMgr::pollConnect()) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }
  }

  // Runtime WiFi watchdog — every 60 s after boot completes.
  // Catches desync where s_connected is true but WiFi.status() disagrees.
  // Auto-reconnect and manual Settings→WiFi→Connect cover all other recovery.
  static unsigned long s_lastWatchdog = 0;
  if (s_mode != BOOT_STATUS && !inOtaMode() && millis() - s_lastWatchdog >= 60000UL) {
    s_lastWatchdog = millis();
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (WifiMgr::isConnected() && !wifiUp) {
#ifndef RELEASE_BUILD
      Serial.printf("[%lums] Watchdog: desync detected, calling reconnect()\n",
                    (unsigned long)millis());
#endif
      WifiMgr::reconnect();
    }
  }

  // Reset DCS-BIOS socket on WiFi disconnect so begin() is re-called on reconnect.
  if (WifiMgr::consumeDisconnect()) {
    s_dcsBiosStarted = false;
  }

  bool dcsActivity = inOtaMode() ? false : DcsBios::process();
  bool dcsLive     = inOtaMode() ? false : DcsBios::isConnected();
  bool mc          = dcsLive && DcsBios::masterCaution();
  bool rwr         = dcsLive && DcsBios::rwrMslLaunch();

  int8_t delta = Encoder::readDelta();
  if (s_encReversed) delta = -delta;
  bool encActivity = (delta != 0) || Encoder::shortPressed() || Encoder::longPressed();

  // Wake OLED on any activity
  if (s_oledSleeping && encActivity) {
    UI::wake();
    s_oledSleeping = false;
    s_lastActivity = millis();
    return;
  }
  if (!s_oledSleeping && (encActivity || dcsActivity)) {
    s_lastActivity = millis();
  }

  // RWR MISSILE LAUNCH — higher priority than MC; 200ms debounce, 100ms flash
  static unsigned long s_rwrHighSince = 0;
  if (rwr && s_rwrHighSince == 0)  s_rwrHighSince = millis();
  if (!rwr)                         s_rwrHighSince = 0;
  bool rwrConfirmed = rwr && (millis() - s_rwrHighSince >= 200);

  if (rwrConfirmed && s_oledSleeping) {
    UI::wake();
    s_oledSleeping = false;
    s_lastActivity = millis();
  }
  if (rwrConfirmed) {
    if (s_mode == BOOT_STATUS) s_mode = homeMode();
    s_rwrActive = true;
    if (millis() - s_rwrFlashTimer > 100) {
      s_rwrFlash = !s_rwrFlash;
      s_rwrFlashTimer = millis();
    }
    UI::showMissileLaunch(s_rwrFlash);
    if (Encoder::shortPressed()) {
      DcsBios::sendCommand(DCSBIOS_CMD_CMDS_DISPENSE, 1);
      delay(100);
      DcsBios::sendCommand(DCSBIOS_CMD_CMDS_DISPENSE, 0);
    }
    return;
  }
  if (s_rwrActive && !rwrConfirmed) s_rwrActive = false;

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
    if (s_mode == BOOT_STATUS) s_mode = homeMode();
    s_mcActive = true;
    if (millis() - s_mcFlashTimer > 200) {
      s_mcFlash = !s_mcFlash;
      s_mcFlashTimer = millis();
    }
    if (s_scState == SC_IDLE) {
      if (DcsBios::storesConfigLight()) {
        UI::showStoresConfig(s_mcFlash);
        if (Encoder::shortPressed()) {
          s_scTarget    = DcsBios::storesConfigSw() ^ 1;
          s_scTPress    = millis();
          s_scTLastSend = millis();
          DcsBios::sendCommand(DCSBIOS_CMD_STORES_CONFIG_SW, s_scTarget);
          s_scState     = SC_WAITING_SW;
        }
      } else {
        s_scTarget = 0xFF;
        UI::showMasterCaution(s_mcFlash);
        if (Encoder::shortPressed()) {
          DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 1);
          delay(100);
          DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 0);
        }
      }
    } else if (s_scState == SC_WAITING_SW) {
      UI::showStoresConfig(s_mcFlash);
      if (DcsBios::storesConfigSw() == s_scTarget) {
        s_scState = SC_WAITING_LIGHT;
      } else if (millis() - s_scTLastSend >= SC_RETRY_MS) {
        DcsBios::sendCommand(DCSBIOS_CMD_STORES_CONFIG_SW, s_scTarget);
        s_scTLastSend = millis();
      }
    } else if (s_scState == SC_WAITING_LIGHT) {
      if (millis() - s_scTPress >= SC_LIGHT_TIMEOUT_MS) {
        s_scState = SC_GAVE_UP;
        UI::showMasterCaution(s_mcFlash);
      } else {
        UI::showStoresConfig(s_mcFlash);
      }
    } else if (s_scState == SC_GAVE_UP) {
      if (!DcsBios::storesConfigLight()) {
        s_scState  = SC_IDLE;
        s_scTarget = 0xFF;
      }
      UI::showMasterCaution(s_mcFlash);
      if (Encoder::shortPressed()) {
        DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 1);
        delay(100);
        DcsBios::sendCommand(DCSBIOS_CMD_MC_RESET, 0);
      }
    }
    return;
  }
  if (s_mcActive && !mcConfirmed) {
    s_mcActive = false;
    s_scState  = SC_IDLE;
    s_scTarget = 0xFF;
  }

  // Normal operation
  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !s_wasDcsConnected) {
    if (s_mode == MACRO_MENU) s_mode = homeMode();
  }
  if (!nowConnected && s_wasDcsConnected) {
    if (s_mode == AIRCRAFT_STATUS || s_mode == NOT_READY) s_mode = BOOT_STATUS;
    if (s_mode == SETUP_RUNNING) { s_setupStep = 0; s_mode = BOOT_STATUS; }
  }
  s_wasDcsConnected = nowConnected;

  // NOT_READY ↔ AIRCRAFT_STATUS when MWS flag changes while DCS stays connected
  bool nowMwsOn = nowConnected && DcsBios::mwsOn();
  if (nowMwsOn && !s_wasMwsOn) {
    if (s_mode == NOT_READY) s_mode = AIRCRAFT_STATUS;
  }
  if (!nowMwsOn && s_wasMwsOn) {
    if (s_mode == AIRCRAFT_STATUS) s_mode = NOT_READY;
  }
  s_wasMwsOn = nowMwsOn;

  // Menu state machine
  if (s_mode == BOOT_STATUS) {
    if (!s_bootStarted) {
      s_bootStarted = WifiMgr::startWifi();
    }

    WifiMgr::WifiPhase ph = WifiMgr::getPhase();

    // Start DCS-BIOS on first IP assignment
    if (ph.ip && !s_dcsBiosStarted) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }

    if (Encoder::longPressed()) {
      s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0;
      return;
    }

    // Timer A: restart startWifi() for reason codes arduino-esp32 will not auto-retry.
    // Reason 1 (UNSPECIFIED) is emitted during arduino-esp32's own reconnect cycle —
    // firing Timer A for reason 1 interrupts a running auto-reconnect.
    // s_bootRetryAt is file-scope so the countdown can be read below for display.
    if (!ph.ssid && ph.failReasonCode != 0 && ph.failReasonCode != 1) {
      if (s_bootRetryAt == 0) {
        s_bootRetryAt = millis() + 30000UL;
#ifndef RELEASE_BUILD
        Serial.printf("[%lums] Timer A armed for reason=%u, fires at %lums\n",
                      (unsigned long)millis(), (unsigned)ph.failReasonCode,
                      (unsigned long)s_bootRetryAt);
#endif
      }
      if (millis() >= s_bootRetryAt) {
#ifndef RELEASE_BUILD
        Serial.printf("[%lums] Timer A FIRED — restarting startWifi()\n",
                      (unsigned long)millis());
#endif
        s_bootRetryAt  = 0;
        s_bootStarted  = false;
      }
    } else {
      if (s_bootRetryAt != 0) {
#ifndef RELEASE_BUILD
        if (ph.failReasonCode == 1) {
          static unsigned long s_suppressLoggedAt = 0;
          if (millis() - s_suppressLoggedAt > 5000UL) {
            s_suppressLoggedAt = millis();
            Serial.printf("[%lums] Timer A suppressed — reason=1 (auto-reconnect running)\n",
                          (unsigned long)millis());
          }
        }
#endif
      }
      s_bootRetryAt = 0;
    }

    if (ph.ip && dcsLive) {
      if (s_bootDoneAt == 0) s_bootDoneAt = millis();
      if (millis() - s_bootDoneAt >= 1500UL || delta != 0 || Encoder::shortPressed()) {
        s_mode = homeMode();
        return;
      }
    } else {
      s_bootDoneAt = 0;
    }

    bool dcs = DcsBios::hasData();

    const char* authMode = nullptr;
    const char* ipOctet  = nullptr;
    static char ipOctetBuf[5];
    if (ph.ssid) {
      authMode = WifiMgr::authModeStr();
    }
    if (ph.ip) {
      snprintf(ipOctetBuf, sizeof(ipOctetBuf), "%u", (unsigned)WiFi.localIP()[3]);
      ipOctet = ipOctetBuf;
    }

    int retrySecs = 0;
    if (s_bootRetryAt > 0) {
      unsigned long remainingMs = (s_bootRetryAt > millis()) ? (s_bootRetryAt - millis()) : 0;
      retrySecs = (int)(remainingMs / 1000UL);
    }

    BootStatusInfo bsi = { ph.ssid, ph.ip, dcs,
                           WifiMgr::failReasonStr(), authMode, ipOctet, retrySecs };
    UI::showBootStatus(bsi);
    return;

  } else if (s_mode == AIRCRAFT_STATUS) {
    if (Encoder::shortPressed()) { s_mode = MACRO_MENU; s_macroLastInput = millis(); }
    if (Encoder::longPressed()) { s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0; }
    if (delta != 0) { s_mode = MACRO_MENU; s_macroLastInput = millis(); }

  } else if (s_mode == NOT_READY) {
    if (Encoder::shortPressed()) {
      s_setupStep = 0;
      s_mode      = SETUP_RUNNING;
    }
    if (Encoder::longPressed()) {
      DcsBios::sendCommand(DCSBIOS_CMD_MWS_SW, 1);
      // s_mode transitions via s_wasMwsOn check when DCS echoes MWS=ON
    }

  } else if (s_mode == SETUP_RUNNING) {
    // Entry: fire initial send on first tick (s_setupStep still 0)
    if (s_setupStep == 0) {
      s_setupStep  = 1;
      s_setupStart = s_setupSent = millis();
      DcsBios::sendCommand(DCSBIOS_CMD_HDPT_SW_L, 1);
    }

    // Verify current step against DCS feedback
    bool confirmed = false;
    switch (s_setupStep) {
      case 1: confirmed = DcsBios::hdptLeft();           break;
      case 2: confirmed = DcsBios::hdptRight();          break;
      case 3: confirmed = DcsBios::cmdsModeKnob() == 3; break;
      case 4: confirmed = DcsBios::rwrPowerLight();      break;
      case 5: confirmed = DcsBios::mwsOn();              break;
      default: break;
    }

    if (confirmed) {
      if (s_setupStep >= 5) {
        s_setupStep = 0;
        s_mode      = AIRCRAFT_STATUS;
      } else {
        s_setupStep++;
        s_setupStart = s_setupSent = millis();
        switch (s_setupStep) {
          case 2: DcsBios::sendCommand(DCSBIOS_CMD_HDPT_SW_R,    1); break;
          case 3: DcsBios::sendCommand(DCSBIOS_CMD_CMDS_MODE_KNB, 3); break;
          case 4: DcsBios::sendCommand(DCSBIOS_CMD_RWR_PWR_BTN,  1); break;
          case 5: DcsBios::sendCommand(DCSBIOS_CMD_MWS_SW,       1); break;
          default: break;
        }
      }
    } else if (millis() - s_setupStart >= SETUP_TIMEOUT_MS) {
      s_setupStep = 0;
      s_mode      = NOT_READY;
    } else if (millis() - s_setupSent >= SETUP_RETRY_MS) {
      s_setupSent = millis();
      switch (s_setupStep) {
        case 1: DcsBios::sendCommand(DCSBIOS_CMD_HDPT_SW_L,     1); break;
        case 2: DcsBios::sendCommand(DCSBIOS_CMD_HDPT_SW_R,     1); break;
        case 3: DcsBios::sendCommand(DCSBIOS_CMD_CMDS_MODE_KNB, 3); break;
        case 4: DcsBios::sendCommand(DCSBIOS_CMD_RWR_PWR_BTN,   1); break;
        case 5: DcsBios::sendCommand(DCSBIOS_CMD_MWS_SW,        1); break;
        default: break;
      }
    }

  } else if (s_mode == MACRO_MENU) {
    if (delta != 0 || Encoder::shortPressed() || Encoder::longPressed())
      s_macroLastInput = millis();
    s_currentMacro = (s_currentMacro + delta + numMacros) % numMacros;
    if (Encoder::shortPressed()) { UI::flashScreen(); executeMacro(s_currentMacro); }
    if (Encoder::longPressed()) {
      s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0;
      UI::flashScreen();
    }

  } else if (s_mode == SETTINGS) {
    s_menuSel = (s_menuSel + delta + 8) % 8;
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
    s_mouseTuneSel = (s_mouseTuneSel + delta + 6) % 6;
    if (s_mouseTuneSel < s_mouseTuneOffset) s_mouseTuneOffset = s_mouseTuneSel;
    if (s_mouseTuneSel >= s_mouseTuneOffset + 3) s_mouseTuneOffset = s_mouseTuneSel - 2;
    if (Encoder::shortPressed()) { UI::flashScreen(); executeMouseTuneItem(); }
    if (Encoder::longPressed()) {
      s_mode = SETTINGS; UI::flashScreen();
    }

  } else if (s_mode == SCREEN_EDIT) {
    s_screenDigits[s_screenDigitPos] = (s_screenDigits[s_screenDigitPos] + delta + 10) % 10;
    if (Encoder::shortPressed()) {
      if (s_screenDigitPos < 7) {
        s_screenDigitPos++;
      } else {
        s_screenW = s_screenDigits[0]*1000 + s_screenDigits[1]*100 +
                    s_screenDigits[2]*10   + s_screenDigits[3];
        s_screenH = s_screenDigits[4]*1000 + s_screenDigits[5]*100 +
                    s_screenDigits[6]*10   + s_screenDigits[7];
        Preferences p; p.begin("brew", false);
        p.putInt("scrW", s_screenW); p.putInt("scrH", s_screenH);
        p.end();
        UI::showSaved();   // includes 600ms delay — enough to read "SAVED"
        safeRestart();
      }
    }
    if (Encoder::longPressed()) { s_mode = MOUSE_TUNE_MENU; UI::flashScreen(); }

  } else if (s_mode == MOUSE_CALIBRATE_X || s_mode == MOUSE_CALIBRATE_Y) {
    if (delta != 0) {
      unsigned long now = millis();
      unsigned long dt  = now - s_lastCalibTick;
      s_lastCalibTick   = now;
      int step;
      if      (dt <  60) step = 40;
      else if (dt < 100) step = 10;
      else if (dt < 200) step = 3;
      else               step = 1;
      if (s_mode == MOUSE_CALIBRATE_X) {
        s_calibX = (uint16_t)constrain((int)s_calibX + delta * step, 0, s_screenW - 1);
      } else {
        s_calibY = (uint16_t)constrain((int)s_calibY + delta * step, 0, s_screenH - 1);
      }
      HID::moveAbs(s_calibX, s_calibY);
      UI::showMouseCalibrate(
        s_mode == MOUSE_CALIBRATE_X ? 0 : 1,
        s_mode == MOUSE_CALIBRATE_X ? s_calibX : s_calibY,
        s_mode == MOUSE_CALIBRATE_X ? kCalibLabelX[s_calibIdx] : kCalibLabelY[s_calibIdx]
      );
    }
    if (Encoder::shortPressed()) {
      if (s_mode == MOUSE_CALIBRATE_X) {
        s_mode = MOUSE_CALIBRATE_Y;
        s_lastCalibTick = millis();
        UI::showMouseCalibrate(1, s_calibY, kCalibLabelY[s_calibIdx]);
      } else {
        mouseParams[s_calibIdx * 2]     = (int)s_calibX;
        mouseParams[s_calibIdx * 2 + 1] = (int)s_calibY;
        {
          Preferences p; p.begin("brew", false);
          switch (s_calibIdx) {
            case 0: p.putInt("apxX",  mouseParams[0]); p.putInt("apxY",  mouseParams[1]); break;
            case 1: p.putInt("amcX2", mouseParams[2]); p.putInt("amcY2", mouseParams[3]); break;
            case 2: p.putInt("lbX2",  mouseParams[4]); p.putInt("lbY2",  mouseParams[5]); break;
            case 3: p.putInt("cdrpX", mouseParams[6]); p.putInt("cdrpY", mouseParams[7]); break;
          }
          p.end();
        }
        s_mode = MOUSE_TUNE_MENU;
      }
    }
    if (Encoder::longPressed()) {
      s_mode = MOUSE_TUNE_MENU;
    }

  } else if (s_mode == WIFI_MENU) {
    s_wifiSubSel = (s_wifiSubSel + delta + 3) % 3;

    if (Encoder::shortPressed()) {
      switch (s_wifiSubSel) {
        case 0:  // Secrets
          s_wifiSubSel = 0;
          WifiMgr::nvsCredentials(s_nvsSsid, sizeof(s_nvsSsid), &s_nvsPassStatus);
          s_mode = SECRETS_MENU;
          break;
        case 1:  // Connect
          WifiMgr::reconnect();
          { unsigned long t0 = millis(); bool ok = false;
            while (millis() - t0 < 15000UL) {
              UI::showWifiConnecting(WifiMgr::activeSSID());
              if (WifiMgr::pollConnect()) { ok = true; break; }
              delay(100);
            }
            if (ok) {
              UI::showWifiConnected(WifiMgr::activeSSID());
              if (!s_dcsBiosStarted) {
                DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                               "255.255.255.255", DCSBIOS_CMD_PORT);
                s_dcsBiosStarted = true;
              }
            } else {
              UI::showWifiFailed(WifiMgr::activeSSID());
              delay(1500);
            }
          }
          s_gStatus = 0; s_gChecked = false;
          break;
        case 2:  // Back
          s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
          s_mode = SETTINGS;
          break;
      }
    }
    if (Encoder::longPressed()) {
      s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
      s_mode = SETTINGS;
    }

  } else if (s_mode == SECRETS_MENU) {
    s_wifiSubSel = (s_wifiSubSel + delta + 4) % 4;

    if (Encoder::shortPressed()) {
      switch (s_wifiSubSel) {
        case 0:  // Zeroize
          WifiMgr::clearOverride();
          UI::showSaved();
          safeRestart();
          break;
        case 1:  // BLE TERM — launch directly, no confirm dialog
          { bool saved = WifiMgr::runBleSetup(
              []() { UI::showBleActive(WifiMgr::isBleClientConnected()); },
              []() { Encoder::readDelta(); return Encoder::longPressed(); });
            if (saved) safeRestart();
          }
          s_wifiSubSel = 0;
          WifiMgr::nvsCredentials(s_nvsSsid, sizeof(s_nvsSsid), &s_nvsPassStatus);
          s_mode = SECRETS_MENU;
          break;
        case 2:  // Scan — stub
          UI::showNotImplemented();
          Encoder::flush();
          while (!Encoder::shortPressed()) { Encoder::readDelta(); delay(10); }
          break;
        case 3:  // Back
          s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
          s_mode = WIFI_MENU;
          break;
      }
    }
    if (Encoder::longPressed()) {
      s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
      s_mode = WIFI_MENU;
    }

  } else if (s_mode == FIRMWARE_CHECKING) {
    if (!WifiMgr::isConnected()) {
      strlcpy(s_otaError, "No WiFi", sizeof(s_otaError));
      s_otaPerformFailed = false;
      s_mode = FIRMWARE_ERROR;
    } else {
      char ver[24];
      snprintf(ver, sizeof(ver), "Current Firmware v%s", FIRMWARE_VERSION);
      UI::showFirmwareChecking(ver, WiFi.RSSI());
      s_otaResult = OTA::check();
      if (s_otaResult.error[0]) {
        strlcpy(s_otaError, s_otaResult.error, sizeof(s_otaError));
        s_otaPerformFailed = false;
        s_mode = FIRMWARE_ERROR;
      } else if (s_otaResult.available) {
        s_mode = FIRMWARE_CONFIRM;
      } else {
        s_mode = FIRMWARE_UP_TO_DATE;
      }
    }

  } else if (s_mode == FIRMWARE_UP_TO_DATE) {
    if (Encoder::shortPressed()) {
      s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2;
    }

  } else if (s_mode == FIRMWARE_CONFIRM) {
    if (Encoder::shortPressed()) s_mode = FIRMWARE_UPDATING;
    if (Encoder::longPressed())  { s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2; }

  } else if (s_mode == FIRMWARE_UPDATING) {
    UI::showFirmwareUpdating(0);
    if (!OTA::perform(s_otaResult.url, otaProgressCb)) {
      strlcpy(s_otaError, OTA::performError(), sizeof(s_otaError));
      s_otaPerformFailed = true;
      s_mode = FIRMWARE_ERROR;
    }

  } else if (s_mode == FIRMWARE_ERROR) {
    if (s_otaPerformFailed) {
      if (Encoder::shortPressed()) { s_otaPerformFailed = false; s_mode = FIRMWARE_CONFIRM; }
      if (Encoder::longPressed())  { s_otaPerformFailed = false; s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2; }
    } else {
      if (Encoder::shortPressed()) { s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2; }
    }
  }

  // Macro menu auto-return: 15 s idle while DCS is streaming → back to aircraft status
  if (s_mode == MACRO_MENU && DcsBios::isConnected() &&
      millis() - s_macroLastInput > 15000UL)
    s_mode = AIRCRAFT_STATUS;

  // Reset sleep timer when returning from OTA flow so the result screen
  // gets a full timeout window regardless of how long the download took.
  {
    static MenuState s_prevMode = BOOT_STATUS;
    if (s_prevMode >= FIRMWARE_CHECKING && s_mode < FIRMWARE_CHECKING)
      s_lastActivity = millis();
    s_prevMode = s_mode;
  }

  // OLED sleep check
  if (!s_oledSleeping && !inOtaMode() && s_sleepSecs > 0 &&
      millis() - s_lastActivity > (unsigned long)s_sleepSecs * 1000UL) {
    UI::sleep();
    s_oledSleeping = true;
  }

  // OLED update — skip while display is powered down
  if (!s_oledSleeping && millis() - s_lastOled > 200) {
    s_lastOled = millis();
    switch (s_mode) {
      case BOOT_STATUS:       break;  // handled inline — returns early before this switch
      case AIRCRAFT_STATUS:
        UI::showAircraftStatus(DcsBios::fuelLbs(),
                               DcsBios::chaffStr(),
                               DcsBios::flareStr(),
                               DcsBios::ecmTransmitting()); break;
      case NOT_READY:
        UI::showNotReady((millis() / 750) % 2 == 0);
        break;
      case SETUP_RUNNING: {
        uint8_t displayStep = (s_setupStep == 0) ? 1 : s_setupStep;
        UI::showSetupRunning(displayStep, (millis() / 250) % 2 == 0);
        break;
      }
      case MACRO_MENU:        UI::showMacroMenu(s_currentMacro); break;
      case SETTINGS:          UI::showSettingsMenu(s_menuSel, s_menuOffset,
                                s_encReversed, WifiMgr::isConnected(),
                                DcsBios::isConnected()); break;
      case BRIGHTNESS_ADJUST: UI::showBrightnessAdjust(s_brightness); break;
      case SLEEP_ADJUST:      UI::showSleepAdjust(s_sleepSecs); break;
      case MOUSE_TUNE_MENU:   UI::showMouseTuneMenu(s_mouseTuneSel, s_mouseTuneOffset); break;
      case MOUSE_CALIBRATE_X:
        UI::showMouseCalibrate(0, s_calibX, kCalibLabelX[s_calibIdx]);
        break;
      case MOUSE_CALIBRATE_Y:
        UI::showMouseCalibrate(1, s_calibY, kCalibLabelY[s_calibIdx]);
        break;
      case SCREEN_EDIT: UI::showScreenEdit(s_screenDigits, s_screenDigitPos); break;
      case WIFI_MENU:
        if (!s_gChecked) {
          s_gStatus  = WifiMgr::checkInternet() ? 1 : 2;
          s_gChecked = true;
        }
        UI::showWifiMenu(s_wifiSubSel,
                         WifiMgr::rssi(),
                         WifiMgr::activeSSID(),
                         WifiMgr::activeIP(),
                         s_gStatus);
        break;
      case SECRETS_MENU:
        UI::showSecretsMenu(s_wifiSubSel, s_nvsSsid, s_nvsPassStatus);
        break;
      case FIRMWARE_CHECKING:  break;  // screen set inline before blocking call
      case FIRMWARE_UP_TO_DATE: {
        char ver[24];
        snprintf(ver, sizeof(ver), "Current Firmware v%s", FIRMWARE_VERSION);
        UI::showFirmwareUpToDate(ver);
        break;
      }
      case FIRMWARE_CONFIRM: {
        char curVer[24];
        snprintf(curVer, sizeof(curVer), "Current Firmware v%s", FIRMWARE_VERSION);
        char availVer[10];
        snprintf(availVer, sizeof(availVer), "v0.%02d", s_otaResult.versionInt);
        UI::showFirmwareConfirm(curVer, availVer);
        break;
      }
      case FIRMWARE_UPDATING:  break;  // screen driven by otaProgressCb
      case FIRMWARE_ERROR:     UI::showFirmwareError(s_otaError, s_otaPerformFailed); break;
    }
  }
}
