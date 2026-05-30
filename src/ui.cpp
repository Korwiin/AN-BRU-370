#include "ui.h"
#include "pins.h"

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

void UI::update() {
  // expanded in later tasks
}
