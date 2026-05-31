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

  bool isConnected();
  const char* activeSSID();

  // Returns device IP as a dotted-decimal string (e.g. "192.168.1.5"), or "--" if not connected.
  const char* activeIP();

  // Reset connected state and reconnect with saved credentials (non-blocking start).
  // startConnect() handles WiFi.disconnect internally — no double-disconnect.
  void reconnect();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();

  // Runs serial credential entry flow.
  // oledCb: called repeatedly while waiting (to keep OLED updated).
  // cancelCb: return true to cancel (long press check).
  // Returns true if credentials saved successfully.
  bool runSerialSetup(void (*oledCb)(), bool (*cancelCb)());

  // Runs BLE UART (Nordic UART Service) credential entry session.
  // Disconnects WiFi, starts NimBLE, presents an interactive terminal prompt.
  // oledActiveCb: called every loop tick while the BLE session is running.
  // cancelCb: return true to abort (long press check).
  // Returns true if credentials saved — caller must call ESP.restart().
  // Returns false if cancelled; WiFi reconnect is handled internally.
  bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());

  // Runs encoder character-scroll entry for a single field.
  // result: filled on success (size maxLen). Returns false if cancelled (long press).
  bool runEncoderEntry(const char* fieldName,
                       char* result, size_t maxLen,
                       int8_t (*deltaFn)(),
                       bool   (*shortFn)(),
                       bool   (*longFn)(),
                       void   (*oledFn)(const char* field, const char* buf, const char* sel));
}
