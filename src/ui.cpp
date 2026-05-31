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
  "Reboot","Hand","Brightness","Sleep","Mouse Tune","WiFi","EXIT"
};
static const int kNumMenuItems = 7;

void UI::showSettingsMenu(int sel, int offset, int hand, bool wifiOk, bool dcsOk) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — status (x=0..63)
  u8g2.drawStr(0, 8,  "AN/BRU-370");
  char ver[12];
  snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
  u8g2.drawStr(0, 16, ver);
  u8g2.drawStr(0, 24, wifiOk ? "WiFi:OK" : "WiFi:--");
  u8g2.drawStr(0, 32, dcsOk  ? "DCS:OK"  : "DCS:--");

  // Right panel — 4-item scrolling menu (x=65..127)
  for (int i = 0; i < 4; i++) {
    int idx = offset + i;
    if (idx >= kNumMenuItems) break;
    int y = 8 + i * 8;
    const char* label;
    if (idx == 1) label = (hand == 0) ? "Hand:Left" : "Hand:Right";
    else          label = s_menuItems[idx];
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
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "BRIGHTNESS");
  u8g2.drawFrame(0, 10, 128, 8);
  int fill = (value * 126) / 255;
  if (fill > 0) u8g2.drawBox(1, 11, fill, 6);
  char buf[5]; snprintf(buf, sizeof(buf), "%d", value);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr((128 - w) / 2, 26, buf);
  u8g2.drawStr(0, 31, "SP=Save LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showSleepAdjust(int secs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "SLEEP TIMER");
  u8g2.drawFrame(0, 10, 128, 8);
  if (secs == 0) {
    u8g2.drawBox(1, 11, 126, 6);
    u8g2.setDrawColor(0);
    int nw = u8g2.getStrWidth("Never");
    u8g2.drawStr((128 - nw) / 2, 17, "Never");
    u8g2.setDrawColor(1);
  } else {
    int fill = ((secs - 10) * 126) / 110;
    if (fill > 0) u8g2.drawBox(1, 11, fill, 6);
    char buf[8]; snprintf(buf, sizeof(buf), "%ds", secs);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - w) / 2, 26, buf);
  }
  u8g2.drawStr(0, 31, "SP=Save LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showMouseTuneMenu(int sel, int offset) {
  static const char* items[] = {
    "PinTool X","PinTool Y","MapCtr X","MapCtr Y",
    "Label X","Label Y","Run:Pin","Run:Ctr","Save+Exit","Cancel"
  };
  static const int kItems = 10;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "MOUSE TUNE");
  for (int i = 0; i < 3; i++) {
    int idx = offset + i;
    if (idx >= kItems) break;
    int y = 18 + i * 8;
    if (idx == sel) { u8g2.drawStr(0, y, ">"); u8g2.drawStr(10, y, items[idx]); }
    else              u8g2.drawStr(10, y, items[idx]);
  }
  u8g2.sendBuffer();
}

void UI::showMouseTuneEdit(int paramIdx, int digits[4], int digitPos) {
  static const char* labels[] = {
    "PinTool X","PinTool Y","MapCtr X","MapCtr Y","Label X","Label Y"
  };
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, labels[paramIdx]);
  u8g2.setFont(u8g2_font_9x15_tr);
  for (int d = 0; d < 4; d++) {
    int x = 20 + d * 22;
    char dc[2] = {(char)('0' + digits[d]), 0};
    if (d == digitPos) {
      u8g2.setDrawColor(1); u8g2.drawBox(x - 1, 10, 20, 16);
      u8g2.setDrawColor(0); u8g2.drawStr(x, 25, dc);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(x, 25, dc);
    }
  }
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 31, digitPos < 3 ? "SP=nxt LP=back" : "SP=done LP=back");
  u8g2.sendBuffer();
}

void UI::showSerialActive() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "SERIAL ACTIVE");
  u8g2.drawStr(0, 24, "LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showWifiSubMenu(int sel) {
  static const char* items[] = {"Serial Entry", "Manual Entry", "Back"};
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "WIFI SETUP");
  for (int i = 0; i < 3; i++) {
    int y = 18 + i * 8;
    if (i == sel) { u8g2.drawStr(0, y, ">"); u8g2.drawStr(10, y, items[i]); }
    else            u8g2.drawStr(10, y, items[i]);
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
