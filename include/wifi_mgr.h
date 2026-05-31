#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // WiFi connection timeout used by the splash loop in main.cpp
  constexpr unsigned long kWifiConnectTimeoutMs = 30000UL;

  // Load credentials and call WiFi.begin() — returns immediately (non-blocking).
  void startConnect();

  // Call each loop tick while connecting. Updates internal connected state on
  // first WL_CONNECTED. Returns true exactly once (the tick connection is made).
  bool pollConnect();

  // Abort connection attempt. Clears WiFi credentials from driver.
  void cancelConnect();

  // Call once in setup(). Reads NVS override first, falls back to config.h defaults.
  // Blocks until connected or 10 s timeout. Returns true if connected.
  bool begin();

  bool isConnected();
  const char* activeSSID();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
  bool runSerialSetup(void (*oledCb)(), bool (*cancelCb)());
  bool runEncoderEntry(const char* fieldName,
                       char* result, size_t maxLen,
                       int8_t (*deltaFn)(),
                       bool   (*shortFn)(),
                       bool   (*longFn)(),
                       void   (*oledFn)(const char* field, const char* buf, const char* sel));
}
