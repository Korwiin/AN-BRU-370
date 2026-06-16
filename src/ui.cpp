#include "ui.h"
#include "macros.h"
#include "pins.h"
#include "config.h"

static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C
  u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

void UI::begin() {
  // u8g2.begin() calls Wire.begin(sda, scl) internally using constructor pins.
  // We must NOT call Wire.begin() here first — double-init leaves Wire broken.
  u8g2.begin();
  Wire.setClock(400000);
  Wire.setTimeOut(100);
  u8g2.setContrast(20);
}

// Not called from setup() — showSplashProgress() is used during boot. Retained for future use.
void UI::showSplash() {
  static const char* text = "AN/BRU-370";
  u8g2.clearBuffer();
  // Try 26px font; fall back if it doesn't fit horizontally
  u8g2.setFont(u8g2_font_logisoso26_tf);
  if (u8g2.getStrWidth(text) > 128)
    u8g2.setFont(u8g2_font_t0_22b_mr);
  int w = u8g2.getStrWidth(text);
  // Text is all-caps — center on cap height, ignoring descender space
  int y = (32 + u8g2.getAscent()) / 2;
  u8g2.drawStr((128 - w) / 2, y, text);
  u8g2.sendBuffer();
}

void UI::showSplashProgress(int fill, bool wifiOk) {
  static const char* text = "AN/BRU-370";
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso26_tf);
  if (u8g2.getStrWidth(text) > 128)
    u8g2.setFont(u8g2_font_t0_22b_mr);
  int w = u8g2.getStrWidth(text);
  int y = (32 + u8g2.getAscent()) / 2;
  u8g2.drawStr((128 - w) / 2, y, text);
  if (wifiOk) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 25, 128, 7);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* msg = "WiFi Connected";
    int mw = u8g2.getStrWidth(msg);
    u8g2.drawStr((128 - mw) / 2, 31, msg);
  } else if (fill > 0) {
    u8g2.drawBox(0, 31, fill, 1);
  }
  u8g2.sendBuffer();
}

void UI::sleep() { u8g2.setPowerSave(1); }
void UI::wake()  { u8g2.setPowerSave(0); }

void UI::showWifiConnecting(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "WIFI CONNECTING");
  u8g2.drawStr(0, 22, ssid);
  u8g2.sendBuffer();
}

void UI::showWifiFailed(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "WIFI FAILED");
  u8g2.drawStr(0, 22, ssid);
  u8g2.sendBuffer();
}

void UI::showWifiConnected(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "WIFI CONNECTED");
  u8g2.drawStr(0, 22, ssid);
  u8g2.sendBuffer();
  delay(1000);
}

void UI::showSyncing() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 16, "SYNCING...");
  u8g2.sendBuffer();
}

void UI::showSynced() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 16, "SYNCED");
  u8g2.sendBuffer();
  delay(800);
}

void UI::showSyncFailed() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 16, "SYNC FAILED");
  u8g2.sendBuffer();
}

void UI::showMasterCaution(bool flashState) {
  u8g2.clearBuffer();
  if (flashState) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 32);
    u8g2.setDrawColor(0);
  }
  u8g2.setFont(u8g2_font_9x15B_tr);
  int w1 = u8g2.getStrWidth("MASTER");
  u8g2.drawStr((128 - w1) / 2, 14, "MASTER");
  int w2 = u8g2.getStrWidth("CAUTION");
  u8g2.drawStr((128 - w2) / 2, 30, "CAUTION");
  if (flashState) u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void UI::showMissileLaunch(bool flashState) {
  u8g2.clearBuffer();
  if (flashState) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 32);
    u8g2.setDrawColor(0);
  }
  u8g2.setFont(u8g2_font_9x15B_tr);
  int w1 = u8g2.getStrWidth("MISSILE");
  u8g2.drawStr((128 - w1) / 2, 14, "MISSILE");
  int w2 = u8g2.getStrWidth("LAUNCH");
  u8g2.drawStr((128 - w2) / 2, 30, "LAUNCH");
  if (flashState) u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void UI::showStoresConfig(bool flashState) {
  u8g2.clearBuffer();
  if (flashState) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 32);
    u8g2.setDrawColor(0);
  }
  u8g2.setFont(u8g2_font_9x15B_tr);
  int w1 = u8g2.getStrWidth("STORES");
  u8g2.drawStr((128 - w1) / 2, 14, "STORES");
  int w2 = u8g2.getStrWidth("CONFIG");
  u8g2.drawStr((128 - w2) / 2, 30, "CONFIG");
  if (flashState) u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}


void UI::showMacroMenu(int idx) {
  Macro* m = &macros[idx];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_22b_mr);
  int w = u8g2.getStrWidth(m->name);
  u8g2.drawStr((128 - w) / 2, 22, m->name);
  u8g2.sendBuffer();
}

void UI::flashScreen() {
  u8g2.setDrawColor(2);
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
  delay(80);
  u8g2.setDrawColor(2);
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void UI::update() {
  // expanded in later tasks
}

// ---- Settings menu ----

static const char* s_menuItems[] = {
  "Knob","Brightness","LCD Sleep","WiFi","Mouse Tune","Firmware","Reboot","EXIT"
};
static const int kNumMenuItems = 8;

void UI::showSettingsMenu(int sel, int offset, bool encReversed, bool wifiOk, bool dcsOk) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — status (x=0..63)
  u8g2.drawStr(0, 8,  "AN/BRU-370");
  char ver[12];
  snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
  u8g2.drawStr(0, 16, ver);
  u8g2.drawStr(0, 24, wifiOk ? "WiFi: OK" : "WiFi: --");
  u8g2.drawStr(0, 32, dcsOk  ? "DCS: OK"  : "DCS: --");

  // Right panel — 4-item scrolling menu (x=65..127)
  for (int i = 0; i < 4; i++) {
    int idx = offset + i;
    if (idx >= kNumMenuItems) break;
    int y = 8 + i * 8;
    const char* label;
    if (idx == 0)      label = encReversed ? "Knob:CCW" : "Knob:CW";
    else if (idx == 1) label = "Bright";
    else if (idx == 4) label = "Mouse";
    else if (idx == 5) label = "Update";
    else               label = s_menuItems[idx];
    if (idx == sel) {
      u8g2.drawStr(65, y, ">");
      u8g2.drawStr(71, y, label);
    } else {
      u8g2.drawStr(71, y, label);
    }
  }
  u8g2.sendBuffer();
}

void UI::showBrightnessAdjust(int value) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — actions (x=0..61)
  u8g2.drawStr(0, 10, "SP=Save");
  u8g2.drawStr(0, 24, "LP=Cancel");

  // Right panel — title, bar, value (x=65..127)
  u8g2.drawStr(65, 8, "Brightness");
  u8g2.drawFrame(65, 11, 62, 8);
  int fill = (value * 60) / 255;
  if (fill > 0) u8g2.drawBox(66, 12, fill, 6);
  char buf[5];
  snprintf(buf, sizeof(buf), "%d", value);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr(65 + (62 - w) / 2, 28, buf);

  u8g2.sendBuffer();
}

void UI::showSleepAdjust(int secs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — actions (x=0..61)
  u8g2.drawStr(0, 10, "SP=Save");
  u8g2.drawStr(0, 24, "LP=Cancel");

  // Right panel — title, bar, value (x=65..127)
  u8g2.drawStr(65, 8, "LCD Timeout");
  u8g2.drawFrame(65, 11, 62, 8);
  if (secs == 0) {
    u8g2.drawBox(66, 12, 60, 6);
    u8g2.setDrawColor(0);
    const char* nv = "Never";
    int nw = u8g2.getStrWidth(nv);
    u8g2.drawStr(65 + (62 - nw) / 2, 17, nv);
    u8g2.setDrawColor(1);
  } else {
    int fill = ((secs - 10) * 60) / 110;
    if (fill > 0) u8g2.drawBox(66, 12, fill, 6);
    char buf[8];
    snprintf(buf, sizeof(buf), "%ds", secs);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr(65 + (62 - w) / 2, 28, buf);
  }
  u8g2.sendBuffer();
}

void UI::showMouseTuneMenu(int sel, int offset) {
  static const char* items[] = {
    "Screen Size",
    "Map Pin Tool POS", "Map Center POS",
    "Pin Label POS", "Click Out POS",
    "Back"
  };
  static const int kItems = 6;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "MOUSE POSITION TUNING");
  for (int i = 0; i < 3; i++) {
    int idx = offset + i;
    if (idx >= kItems) break;
    int y = 18 + i * 8;
    if (idx == sel) { u8g2.drawStr(0, y, ">"); u8g2.drawStr(10, y, items[idx]); }
    else              u8g2.drawStr(10, y, items[idx]);
  }
  u8g2.sendBuffer();
}

void UI::showMouseCalibrate(int axis, uint16_t val, const char* label) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, label);
  char vbuf[14];
  snprintf(vbuf, sizeof(vbuf), "%c: %u", axis == 0 ? 'X' : 'Y', (unsigned)val);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 19, vbuf);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 31, axis == 0 ? "SP=lock  LP=cancel" : "SP=save  LP=cancel");
  u8g2.sendBuffer();
}

void UI::showScreenEdit(int digits[8], int digitPos) {
  static const int kDx[8] = {21, 30, 39, 48, 72, 81, 90, 99};
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "Screen");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(59, 20, "x");
  for (int d = 0; d < 8; d++) {
    int x = kDx[d];
    char dc[2] = {(char)('0' + digits[d]), 0};
    if (d == digitPos) {
      u8g2.setDrawColor(1); u8g2.drawBox(x - 1, 9, 8, 12);
      u8g2.setDrawColor(0); u8g2.drawStr(x, 20, dc);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(x, 20, dc);
    }
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 31, digitPos < 7 ? "SP=nxt LP=cancel" : "SP=save LP=cancel");
  u8g2.sendBuffer();
}

void UI::showSerialActive() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "SERIAL ACTIVE");
  u8g2.drawStr(0, 24, "LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showWifiConfirm() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "BLE");
  u8g2.drawStr(0, 16, "Setup");
  u8g2.drawStr(65,  8, "WiFi will");
  u8g2.drawStr(65, 16, "disconnect.");
  u8g2.drawStr(65, 24, "SP=OK");
  u8g2.drawStr(65, 32, "LP=No");
  u8g2.sendBuffer();
}

void UI::showBleActive() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "BLE");
  u8g2.drawStr(0, 16, "Active");
  u8g2.drawStr(65,  8, "AN/BRU-370");
  u8g2.drawStr(65, 16, "Waiting...");
  u8g2.drawStr(65, 24, "LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip, bool wifiEnabled) {
  static const char* items[] = {"Manual", "Bluetooth", "Connect", "Back"};
  static const int kItems = 5;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — WiFi status (x=0..63)
  char ssidLine[13], ipLine[16];
  snprintf(ssidLine, sizeof(ssidLine), "S:%.10s", ssid);
  const char* after2 = ip;
  int dots = 0;
  for (const char* p = ip; *p; p++) {
    if (*p == '.' && ++dots == 2) { after2 = p + 1; break; }
  }
  snprintf(ipLine, sizeof(ipLine), "IP: x.x.%s", after2);
  u8g2.drawStr(0,  8, "WiFi");
  u8g2.drawStr(0, 16, ssidLine);
  u8g2.drawStr(0, 24, ipLine);

  // Right panel — 5-item scrolling menu (x=65..127)
  for (int i = 0; i < 4; i++) {
    int idx = offset + i;
    if (idx >= kItems) break;
    int y = 8 + i * 8;
    const char* label = (idx == 0) ? (wifiEnabled ? "WiFi:ON" : "WiFi:OFF") : items[idx - 1];
    if (idx == sel) {
      u8g2.drawStr(65, y, ">");
      u8g2.drawStr(71, y, label);
    } else {
      u8g2.drawStr(71, y, label);
    }
  }
  u8g2.sendBuffer();
}

void UI::showCharEntry(const char* field, const char* buf, const char* selLabel) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, field);

  // Show current buffer (truncated to ~21 chars @ 6px each)
  char display[22] = {0};
  strncpy(display, buf, 21);
  u8g2.drawStr(0, 19, display);

  // Show selected character in a box at bottom right
  u8g2.drawFrame(104, 20, 22, 12);
  u8g2.drawStr(108, 30, selLabel);
  u8g2.sendBuffer();
}

void UI::showSaved() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15B_tr);
  int w = u8g2.getStrWidth("SAVED");
  u8g2.drawStr((128 - w) / 2, 20, "SAVED");
  u8g2.sendBuffer();
  delay(600);
}

void UI::setContrast(uint8_t value) {
  u8g2.setContrast(value);
}

// ---- Firmware update screens ----

void UI::showFirmwareChecking(const char* currentVer, int rssi) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, "Checking for updates...");
  char rssiLine[16];
  snprintf(rssiLine, sizeof(rssiLine), "WiFi: %ddBm", rssi);
  u8g2.drawStr(0, 24, rssiLine);
  u8g2.sendBuffer();
}

void UI::showFirmwareUpToDate(const char* currentVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, "You are up to date.");
  u8g2.drawStr(0, 24, "SP=Back");
  u8g2.sendBuffer();
}

void UI::showFirmwareConfirm(const char* currentVer, const char* availVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  char line2[24];
  snprintf(line2, sizeof(line2), "Firmware %s avail", availVer);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, line2);
  u8g2.drawStr(0, 24, "SP=Update   LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showFirmwareUpdating(int percent) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8, "Updating...");
  u8g2.drawFrame(0, 13, 128, 8);
  int fill = (percent * 126) / 100;
  if (fill > 0) u8g2.drawBox(1, 14, fill, 6);
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", percent);
  u8g2.drawStr((128 - u8g2.getStrWidth(pct)) / 2, 30, pct);
  u8g2.sendBuffer();
}

void UI::showFirmwareError(const char* reason, bool canRetry) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "Update failed");
  u8g2.drawStr(0, 21, reason);
  u8g2.drawStr(0, 31, canRetry ? "SP=Retry  LP=Back" : "SP=Back");
  u8g2.sendBuffer();
}
