#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "wifi_mgr.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);

  UI::begin();
  UI::showWifiConnecting(WIFI_SSID_DEFAULT);

  bool ok = WifiMgr::begin();
  if (ok) {
    Serial.printf("WiFi connected: %s\n", WifiMgr::activeSSID());
    UI::showWifiConnected(WifiMgr::activeSSID());
  } else {
    Serial.printf("WiFi failed: %s\n", WifiMgr::activeSSID());
    UI::showWifiFailed(WifiMgr::activeSSID());
  }
}

void loop() {
}
