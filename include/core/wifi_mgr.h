#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // Prepares WiFi for a connection attempt, then calls WiFi.begin(ssid, pass).
  // full=true:  WiFi.mode(WIFI_OFF) → WiFi.mode(WIFI_STA) → WiFi.begin()   [full teardown]
  // full=false: WiFi.disconnect()   →                         WiFi.begin()   [soft restart]
  // Returns false if NVS has no SSID (caller should go to Secrets setup).
  bool beginConnect(bool full);

  // Session-only auto-reconnect toggle (not persisted to NVS).
  // Defaults to true (arduino-esp32 default). Apply before calling beginConnect().
  void setAutoReconnect(bool on);
  bool getAutoReconnect();

  // True when WiFi.status() == WL_CONNECTED.
  bool isConnected();

  // SSID last passed to WiFi.begin() (populated by beginConnect()).
  const char* activeSSID();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
  bool hasCredentials();
  void nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus);

  bool isBleClientConnected();

  // BLE UART (Nordic UART Service) credential entry session.
  // Returns true if credentials saved — caller must call ESP.restart().
  bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
}
