#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Passed to UI::showBootStatus() each frame during boot.
struct BootStatusInfo {
  bool rf, ssid, eth, ip, dns, dcs;
  bool rfFail, ssidFail;   // show X instead of -- for these phases
  int attempt;             // 1-3; 0 = USB settle (show all --)
  bool failed;             // all 3 attempts exhausted
  const char* failReason;  // nullptr if no failure yet
};

namespace UI {
  void begin();

  // Renders six-phase WiFi/DCS boot status screen. Call each loop() tick during BOOT_STATUS.
  void showBootStatus(const BootStatusInfo& s);
  void sleep();        // power down OLED (setPowerSave 1)
  void wake();         // power up OLED (setPowerSave 0)

  void showNoCredentials();
  void showWifiConnecting(const char* ssid);
  void showWifiFailed(const char* ssid);
  void showWifiConnected(const char* ssid);  // shows for 1 s then returns

  // Full-screen MASTER CAUTION takeover. Call repeatedly while MC is active.
  // flashState alternates true/false every ~200 ms to produce flash effect.
  void showMasterCaution(bool flashState);

  // Full-screen MISSILE LAUNCH takeover. Call repeatedly while RWR MSL is active.
  // flashState alternates true/false every ~100 ms to produce flash effect.
  void showMissileLaunch(bool flashState);
  void showStoresConfig(bool flashState);

  void showAircraftStatus(uint32_t fuelLbs,
                          const char* chaff, const char* flare, bool ecmTx,
                          bool gearN, bool gearL, bool gearR,
                          uint16_t speedbrake);
  void showMacroMenu(int idx);  // renders current macro name on 128x32 OLED
  void flashScreen();            // brief invert flash for button feedback

  void showSettingsMenu(int sel, int offset, bool encReversed, bool wifiOk, bool dcsOk);
  void showBrightnessAdjust(int value);
  void showSleepAdjust(int secs);
  void showMouseTuneMenu(int sel, int offset);
  void showMouseCalibrate(int axis, uint16_t val, const char* label);
  void showScreenEdit(int digits[8], int digitPos);
  void showSaved();
  void showSerialActive();
  void showBleActive(bool connected);    // displayed while NimBLE UART session is running
  void showWifiMenu(int sel, int rssi, const char* ssid, const char* ip,
                    bool wifiEnabled, uint8_t gStatus);
  void showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus);
  void showNotImplemented();
  // OTA firmware update screens
  void showFirmwareChecking(const char* currentVer, int rssi);
  void showFirmwareUpToDate(const char* currentVer);
  void showFirmwareConfirm(const char* currentVer, const char* availVer);
  void showFirmwareUpdating(int percent);
  void showFirmwareError(const char* reason, bool canRetry = false);
  void setContrast(uint8_t value);

  void update();  // expanded in later tasks
}
