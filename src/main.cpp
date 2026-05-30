#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"
#include "hardware.h"
#include "encoder.h"

static bool wasDcsConnected = false;
static bool syncDone        = false;

// MASTER CAUTION state
static bool s_mcActive    = false;
static bool s_mcFlash     = false;
static unsigned long s_mcFlashTimer = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);

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
  Encoder::readDelta();  // must call every loop to keep button state current

  // MASTER CAUTION — full-screen takeover, blocks all other display/action
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
    return;  // skip rest of loop while MC active
  }
  if (s_mcActive && !mc) {
    s_mcActive = false;  // DCS-BIOS confirmed light off
    Serial.println("MC cleared");
  }

  // Normal operation
  Hardware::update();

  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !wasDcsConnected) {
    UI::showSyncing();
    delay(800);
    UI::showSynced();
    syncDone = true;
  }
  if (!nowConnected && syncDone) {
    syncDone = false;
    UI::showSyncFailed();
  }
  wasDcsConnected = nowConnected;

  if (nowConnected &&
      DcsBios::apPitchSwitch() != 0xFF &&
      DcsBios::apPitchSwitch() != Hardware::sw1Pos()) {
    Serial.printf("AP_PITCH mismatch: physical=%d dcs=%d\n",
                  Hardware::sw1Pos(), DcsBios::apPitchSwitch());
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    Serial.printf("DCS: %s | PITCH=%d ROLL=%d MC=%d | HW: sw1=%d sw2=%d pot=%d\n",
      DcsBios::isConnected() ? "conn" : "wait",
      DcsBios::apPitchSwitch(), DcsBios::apRollSwitch(), DcsBios::masterCaution(),
      Hardware::sw1Pos(), Hardware::sw2Pos(), Hardware::potRaw());
  }
}
