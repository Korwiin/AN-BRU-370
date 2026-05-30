#include "ui.h"
#include "macros.h"
#include "pins.h"
#include "config.h"

static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C
  u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

void UI::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  u8g2.begin();
  u8g2.setContrast(20);
}

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
  if (m->isTwoLine) {
    int w1 = u8g2.getStrWidth(m->name);
    u8g2.drawStr((128 - w1) / 2, 18, m->name);
    int w2 = u8g2.getStrWidth(m->line2);
    u8g2.drawStr((128 - w2) / 2, 31, m->line2);
  } else {
    int w = u8g2.getStrWidth(m->name);
    u8g2.drawStr((128 - w) / 2, 22, m->name);
  }
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

void UI::showSettingsMenu(int sel, int offset, int hand, bool usbReady) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "v%s %s", FIRMWARE_VERSION,
           usbReady ? "USB:OK" : "USB:--");
  u8g2.drawStr(0, 8, hdr);
  for (int i = 0; i < 3; i++) {
    int idx = offset + i;
    if (idx >= kNumMenuItems) break;
    int y = 18 + i * 8;
    const char* label;
    if (idx == 1) label = (hand == 0) ? "Hand:Left" : "Hand:Right";
    else          label = s_menuItems[idx];
    if (idx == sel) { u8g2.drawStr(0, y, ">"); u8g2.drawStr(10, y, label); }
    else              u8g2.drawStr(10, y, label);
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
