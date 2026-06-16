#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // Per-attempt phase flags — set by event handler or beginAttempt().
  // Read via getPhase() which takes a snapshot of the volatile flags.
  struct WifiPhase {
    bool rf;              // radio initialised + WL_IDLE_STATUS confirmed
    bool ssid;            // our SSID found in blocking scan
    bool eth;             // Layer 2 association (WIFI_STA_CONNECTED event)
    bool ip;              // DHCP complete (WIFI_STA_GOT_IP event)
    bool dns;             // DNS server assigned (checked at GOT_IP)
    bool rfFail;          // RF check failed (bad MAC or stale WL_CONNECTED)
    bool ssidFail;        // our SSID not found in scan
    uint8_t failReasonCode; // from WIFI_STA_DISCONNECTED reason field (0 = none)
  };

  // Start one connection attempt. Blocks ~500 ms (radio cycle) + ~2-3 s (scan).
  // Resets all phase flags, cycles radio, checks RF, scans for SSID.
  // If SSID found, calls WiFi.begin() and returns true; caller polls getPhase().ip.
  // Returns false if RF check or SSID scan fails (check getPhase().rfFail/.ssidFail).
  bool beginAttempt(int n);

  // Snapshot of current phase flags (safe to call from loop() tick).
  WifiPhase getPhase();

  // Human-readable failure string derived from phase flags and failReasonCode.
  // Returns nullptr if no failure has occurred yet.
  const char* failReasonStr();

  // Returns true when IP is assigned (uses phase flag; updated by event handler).
  bool isConnected();

  // Returns true exactly once after isConnected() becomes true.
  // Resets if WiFi drops. Used to trigger DCS-BIOS start in non-boot paths.
  bool pollConnect();

  // Abort connection attempt. Clears WiFi state.
  void cancelConnect();

  const char* activeSSID();

  // Returns device IP as "192.168.1.5" or "--" if not connected.
  const char* activeIP();

  // Silent runtime reconnect. Called by watchdog and Settings→Connect.
  void reconnect();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
  bool hasCredentials();
  bool isBleClientConnected();

  int  rssi();
  bool checkInternet();
  void nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus);

  // BLE UART (Nordic UART Service) credential entry session.
  // Returns true if credentials saved — caller must call ESP.restart().
  bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
}
