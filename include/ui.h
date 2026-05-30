#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace UI {
  void begin();

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

  void showSettingsMenu(int sel, int offset, int hand, bool usbReady);
  void showBrightnessAdjust(int value);
  void showSleepAdjust(int secs);
  void showMouseTuneMenu(int sel, int offset);
  void showMouseTuneEdit(int paramIdx, int digits[4], int digitPos);
  void showSaved();
  void showSerialActive();
  void showWifiSubMenu(int sel);
  void setContrast(uint8_t value);

  void update();  // expanded in later tasks
}
