#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // Call once in setup(). Reads NVS override first, falls back to config.h defaults.
  // Blocks until connected or 10 s timeout. Returns true if connected.
  bool begin();

  bool isConnected();
  const char* activeSSID();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
}
