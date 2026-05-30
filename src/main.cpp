#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"
#include "hardware.h"

static bool wasDcsConnected = false;
static bool syncDone        = false;

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
}

void loop() {
  DcsBios::update();
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

  // Log sync mismatch (rare AP magnetic disengage scenario)
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
