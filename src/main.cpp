#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"
#include "dcs_bios.h"

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
    // Broadcast to LAN — DCS PC receives on standard DCS-BIOS cmd port
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   "255.255.255.255", DCSBIOS_CMD_PORT);
    UI::showSyncing();
  } else {
    Serial.printf("WiFi failed: %s\n", WifiMgr::activeSSID());
    UI::showWifiFailed(WifiMgr::activeSSID());
  }
}

void loop() {
  DcsBios::update();

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    Serial.printf("DCS-BIOS: %s | AP_PITCH=%d AP_ROLL=%d MC=%d\n",
      DcsBios::isConnected() ? "connected" : "waiting",
      DcsBios::apPitchSwitch(), DcsBios::apRollSwitch(),
      DcsBios::masterCaution());
  }
}
