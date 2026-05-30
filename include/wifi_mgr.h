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

  // Runs serial credential entry flow.
  // oledCb: called repeatedly while waiting (to keep OLED updated).
  // cancelCb: return true to cancel (long press check).
  // Returns true if credentials saved successfully.
  bool runSerialSetup(void (*oledCb)(), bool (*cancelCb)());

  // Runs encoder character-scroll entry for a single field.
  // result: filled on success (size maxLen). Returns false if cancelled (long press).
  bool runEncoderEntry(const char* fieldName,
                       char* result, size_t maxLen,
                       int8_t (*deltaFn)(),
                       bool   (*shortFn)(),
                       bool   (*longFn)(),
                       void   (*oledFn)(const char* field, const char* buf, const char* sel));
}
