#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // Per-attempt phase flags — set by event handler or startWifi().
  // Read via getPhase() which takes a snapshot of the volatile flags.
  struct WifiPhase {
    bool rf;              // radio initialised + MAC confirmed
    bool ssid;            // our SSID found; confirmed by successful association
    bool eth;             // Layer 2 association (WIFI_STA_CONNECTED event)
    bool ip;              // DHCP complete (WIFI_STA_GOT_IP event)
    bool dns;             // DNS server assigned (checked at GOT_IP)
    bool rfFail;          // RF check failed (bad MAC)
    bool ssidFail;        // no credentials stored
    uint8_t failReasonCode; // from WIFI_STA_DISCONNECTED reason field (0 = none)
  };

  // Starts WiFi in STA mode with auto-reconnect enabled. Call once at boot.
  // Returns false if no credentials or RF check fails.
  bool startWifi();

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

  // Silent runtime reconnect. Called by watchdog for driver-desync (stays in WIFI_STA).
  void reconnect();

  // Returns true (once) after a WiFi disconnect event — caller should
  // tear down any sockets that depend on the WiFi link.
  bool consumeDisconnect();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
  bool hasCredentials();
  bool isBleClientConnected();

  int  rssi();

  // Returns "WPA2" or "WPA3" based on connected AP's auth mode.
  // Only valid when isConnected(). Returns nullptr if not connected.
  const char* authModeStr();

  bool checkInternet();
  void nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus);

  // BLE UART (Nordic UART Service) credential entry session.
  // Returns true if credentials saved — caller must call ESP.restart().
  bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
}
