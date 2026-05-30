#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"
#include "hardware.h"
#include "encoder.h"
#include "hid.h"
#include "macros.h"

static bool wasDcsConnected = false;
static bool syncDone        = false;
static bool s_mcActive      = false;
static bool s_mcFlash       = false;
static unsigned long s_mcFlashTimer = 0;
static int  s_currentMacro  = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);

  HID::begin();
  UI::begin();
  UI::showWifiConnecting(WIFI_SSID_DEFAULT);

  bool wifiOk = WifiMgr::begin();
  if (wifiOk) {
    Serial.printf("WiFi connected: %s\n", WifiMgr::activeSSID());
    UI::showWifiConnected(WifiMgr::activeSSID());
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   "255.255.255.255", DCSBIOS_CMD_PORT);
    UI::showSyncing();
  } else {
    Serial.printf("WiFi failed: %s\n", WifiMgr::activeSSID());
    UI::showWifiFailed(WifiMgr::activeSSID());
  }

  Hardware::begin();
  Encoder::begin();
}

void loop() {
  DcsBios::update();
  int8_t delta = Encoder::readDelta();

  // MASTER CAUTION — full-screen takeover
  bool mc = DcsBios::masterCaution();
  if (mc) {
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
      Serial.println("MC reset sent");
    }
    return;
  }
  if (s_mcActive && !mc) {
    s_mcActive = false;
    Serial.println("MC cleared");
  }

  // Normal operation
  Hardware::update();

  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !wasDcsConnected) {
    UI::showSyncing(); delay(800); UI::showSynced();
    syncDone = true;
  }
  if (!nowConnected && syncDone) {
    syncDone = false;
    UI::showSyncFailed();
  }
  wasDcsConnected = nowConnected;

  if (nowConnected && DcsBios::apPitchSwitch() != 0xFF &&
      DcsBios::apPitchSwitch() != Hardware::sw1Pos()) {
    Serial.printf("AP_PITCH mismatch: physical=%d dcs=%d\n",
                  Hardware::sw1Pos(), DcsBios::apPitchSwitch());
  }

  // Macro menu
  s_currentMacro = (s_currentMacro + delta + numMacros) % numMacros;

  if (Encoder::shortPressed()) {
    UI::flashScreen();
    executeMacro(s_currentMacro);
  }

  // OLED update at 200 ms
  static unsigned long lastOled = 0;
  if (millis() - lastOled > 200) {
    lastOled = millis();
    UI::showMacroMenu(s_currentMacro);
  }
}
