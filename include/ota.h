#pragma once
#include <Arduino.h>

namespace OTA {
  struct CheckResult {
    bool     available;
    uint16_t versionBCD;  // e.g. 0x0015; 0 on parse error
    char     url[256];    // release asset URL from manifest
    char     error[24];   // empty string on success; reason string on failure
  };

  // Fetch manifest from OTA_MANIFEST_URL and compare against FIRMWARE_VERSION_BCD.
  // Blocking (~1-3 s). Returns CheckResult with error[0] != 0 on any failure.
  CheckResult check();

  // Download binary from url and flash via ESP32 Update library.
  // Calls progress(0..100) periodically for OLED updates.
  // Blocking (tens of seconds). Calls ESP.restart() on success — never returns true.
  // Returns false on failure; call performError() to get the reason.
  bool perform(const char* url, void(*progress)(int));
  const char* performError();  // empty string on success
}
