#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace UI {
  void begin();
  void sleep();        // power down OLED (setPowerSave 1)
  void wake();         // power up OLED (setPowerSave 0)

  // Boot / connection screens
  void showStarting();                      // device name + version; shown during USB settle
  void showNoCredentials();                 // "No WiFi Setup / Press to cont."
  void showWifiConnecting(const char* ssid);  // "WIFI CONNECTING / <ssid>"
  void showNoWifi();                        // "No WiFi / SP:Retry LP:Settings" (timeout)
  void showWaitingDcs();                    // "Waiting for DCS... / LP=Settings"

  // Alert takeovers (called in a loop while active)
  void showMasterCaution(bool flashState);
  void showMissileLaunch(bool flashState);
  void showChaffCount(const char* chaffStr, bool flashState);
  void showStoresConfig(bool flashState);

  // Operational screens
  void showAircraftStatus(uint32_t fuelLbs);
  void showNotReady(bool flashState);
  void showSetupRunning(uint8_t step, bool blinkOn);
  void showMacroMenu(int idx);
  void flashScreen();

  // Settings screens
  void showSettingsMenu(int sel, int offset, bool encReversed, bool wifiOk, bool dcsOk);
  void showBrightnessAdjust(int value);
  void showSleepAdjust(int secs);
  void showMouseTuneMenu(int sel, int offset);
  void showMouseCalibrate(int axis, uint16_t val, const char* label);
  void showScreenEdit(int digits[8], int digitPos);
  void showSaved();
  void showRebootCountdown(int secs);
  void showUsbFlashCountdown(int secs);
  void showUsbFlashMode();     // static screen left on OLED while in ROM download mode
  void showUsbFlashUnavailable();  // dev-build guard: item is production-only
  void showBleActive(bool connected);

  // WiFi menu: 5 items (Full Restart / Soft Restart / Auto-Rec / Secrets / Back)
  // sel=selected index (0-4), offset=scroll offset, wifiOk/dcsOk=status indicators,
  // autoReconnect=current auto-reconnect state (shown in "Auto" label)
  void showWifiMenu(int sel, int offset, bool wifiOk, bool dcsOk, bool autoReconnect);

  void showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus);
  void showNotImplemented();

  // OTA firmware update screens
  void showFirmwareChecking(const char* currentVer, int rssi);
  void showFirmwareUpToDate(const char* currentVer);
  void showFirmwareConfirm(const char* currentVer, const char* availVer);
  void showFirmwareUpdating(int percent);
  void showFirmwareError(const char* reason, bool canRetry = false);
  void setContrast(uint8_t value);
}
