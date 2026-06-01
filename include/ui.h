#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace UI {
  void begin();

  void showSplash();   // boot splash with centered version string
  // Redraws splash title with bottom-row WiFi status.
  // wifiOk=false: draws a 1px progress bar of `fill` pixels (0..128) at y=31.
  // wifiOk=true:  clears bottom strip and shows "WiFi Connected" centered at y=31.
  void showSplashProgress(int fill, bool wifiOk);
  void sleep();        // power down OLED (setPowerSave 1)
  void wake();         // power up OLED (setPowerSave 0)

  void showWifiConnecting(const char* ssid);
  void showWifiFailed(const char* ssid);
  void showWifiConnected(const char* ssid);  // shows for 1 s then returns

  void showSyncing();
  void showSynced();
  void showSyncFailed();

  // Full-screen MASTER CAUTION takeover. Call repeatedly while MC is active.
  // flashState alternates true/false every ~200 ms to produce flash effect.
  void showMasterCaution(bool flashState);

  void showMacroMenu(int idx);  // renders current macro name on 128x32 OLED
  void flashScreen();            // brief invert flash for button feedback

  void showSettingsMenu(int sel, int offset, bool encReversed, bool wifiOk, bool dcsOk);
  void showBrightnessAdjust(int value);
  void showSleepAdjust(int secs);
  void showMouseTuneMenu(int sel, int offset);
  void showMouseTuneEdit(int paramIdx, int digits[4], int digitPos);
  void showSaved();
  void showSerialActive();
  // Split layout matching showSettingsMenu: left panel shows WiFi/SSID/IP; right panel scrolls 4 options.
  void showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip);
  // Shows character entry screen.
  // field: "SSID" or "Password"
  // buf: current entered string (null-terminated)
  // selLabel: label for currently highlighted char: single char, "DEL", or "OK"
  void showCharEntry(const char* field, const char* buf, const char* selLabel);
  void setContrast(uint8_t value);

  void update();  // expanded in later tasks
}
