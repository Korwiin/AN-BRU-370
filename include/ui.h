#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace UI {
  void begin();

  void showWifiConnecting(const char* ssid);
  void showWifiFailed(const char* ssid);
  void showWifiConnected(const char* ssid);  // shows for 1 s then returns

  void showSyncing();
  void showSynced();
  void showSyncFailed();

  // Full-screen MASTER CAUTION takeover. Call repeatedly while MC is active.
  // flashState alternates true/false every ~200 ms to produce flash effect.
  void showMasterCaution(bool flashState);

  void update();  // expanded in later tasks
}
