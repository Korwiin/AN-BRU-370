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

// Draws a 5x5 checkmark at (x, y) where y is the text baseline.
static void drawCheck(int x, int y) {
  u8g2.drawLine(x,   y-2, x+2, y);
  u8g2.drawLine(x+2, y,   x+5, y-4);
}

// Draws a 5x5 X mark at (x, y) where y is the text baseline.
static void drawCross(int x, int y) {
  u8g2.drawLine(x,   y-4, x+4, y);
  u8g2.drawLine(x+4, y-4, x,   y);
}

// Draws a phase indicator symbol. done→check, fail→X, inProgress+blink→"..", else "--".
static void drawPhase(int x, int y, bool done, bool fail, bool inProgress) {
  if (fail) {
    drawCross(x, y);
  } else if (done) {
    drawCheck(x, y);
  } else if (inProgress && ((millis() % 600) < 300)) {
    u8g2.drawStr(x, y, "..");
  } else {
    u8g2.drawStr(x, y, "--");
  }
}

void UI::showBootStatus(const BootStatusInfo& s) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Line 1 (y=8): title left, firmware version right
  u8g2.drawStr(0, 8, "AN/BRU-370");
  {
    char ver[10];
    snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
    int vw = u8g2.getStrWidth(ver);
    u8g2.drawStr(128 - vw, 8, ver);
  }

  // Layout: left=RF:/IP:, center=SSID:/DNS:, right=ETH:/DCS:
  // kSymW = max symbol width (2-char string at 5px/char = 10px)
  const int kGap   = 1;
  const int kSymW  = 10;
  const int kRight = 128 - kSymW;   // x of right-column symbols

  // Center column: anchor to the widest center item (SSID: + sym = 36px)
  const int wSSID  = u8g2.getStrWidth("SSID:");
  const int wDNS   = u8g2.getStrWidth("DNS:");
  const int wETH   = u8g2.getStrWidth("ETH:");
  const int wDCS   = u8g2.getStrWidth("DCS:");
  const int wRF    = u8g2.getStrWidth("RF:");
  const int wIP    = u8g2.getStrWidth("IP:");
  const int kCtr   = (128 - (wSSID + kGap + kSymW)) / 2;  // left edge of center column

  // Line 2 (y=16): RF: left | SSID: center | ETH: right
  u8g2.drawStr(0,                     16, "RF:");
  u8g2.drawStr(kCtr,                  16, "SSID:");
  u8g2.drawStr(kRight - kGap - wETH, 16, "ETH:");

  // Line 3 (y=24): IP: left | DNS: center | DCS: right
  u8g2.drawStr(0,                     24, "IP:");
  u8g2.drawStr(kCtr,                  24, "DNS:");
  u8g2.drawStr(kRight - kGap - wDCS, 24, "DCS:");

  bool connecting = (s.attempt > 0 && !s.failed && !s.ip);

  // Phase symbols — blink the current bottleneck
  drawPhase(wRF  + kGap,        16, s.rf,   s.rfFail,   connecting && !s.rf && !s.rfFail);
  drawPhase(kCtr + wSSID + kGap,16, s.ssid, s.ssidFail, connecting && s.rf && !s.ssid && !s.ssidFail);
  drawPhase(kRight,             16, s.eth,  false,      connecting && s.ssid && !s.eth);
  drawPhase(wIP  + kGap,        24, s.ip,   false,      connecting && s.eth && !s.ip);
  drawPhase(kCtr + wDNS + kGap, 24, s.dns,  false,      false);
  drawPhase(kRight,             24, s.dcs,  false,      false);

  // Line 4 (y=32): status text
  static const char kSpin[] = { '-', '\\', '|', '/' };
  if (s.attempt == 0) {
    // USB settle — blank
  } else if (s.failed) {
    char line[32];
    const char* reason = s.failReason ? s.failReason : "WiFi error";
    snprintf(line, sizeof(line), "%.16s LP=Set", reason);
    u8g2.drawStr(0, 32, line);
  } else if (s.ip && !s.dcs) {
    char line[22];
    snprintf(line, sizeof(line), "Waiting for DCS %c", kSpin[(millis() / 1000) % 4]);
    int w = u8g2.getStrWidth(line);
    u8g2.drawStr((128 - w) / 2, 32, line);
  } else if (s.ip) {
    // DCS live — blank
  } else if (s.attempt > 1) {
    char line[28];
    snprintf(line, sizeof(line), "Attempt %d/3  Retrying...", s.attempt);
    u8g2.drawStr(0, 32, line);
  } else {
    u8g2.drawStr(0, 32, "Connecting...");
  }

  u8g2.sendBuffer();
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


void UI::showAircraftStatus(uint16_t fuelLbs, uint8_t chaff, uint8_t flare, bool ecmTx) {
  u8g2.clearBuffer();

  // --- Top zone: fuel number, large font, centered ---
  u8g2.setFont(u8g2_font_t0_22b_mr);
  char fuelStr[8];
  snprintf(fuelStr, sizeof(fuelStr), "%u", (unsigned)fuelLbs);
  int fw = u8g2.getStrWidth(fuelStr);
  int fy = (32 + u8g2.getAscent()) / 2;  // vertically center in top 75%
  u8g2.drawStr((128 - fw) / 2, fy, fuelStr);

  // --- Bottom zone: CH / FL / JAMMING row ---
  u8g2.setFont(u8g2_font_5x7_tr);

  // Format CH and FL strings
  char chStr[8], flStr[8];
  if (chaff == 0xFF) snprintf(chStr, sizeof(chStr), "CH: --");
  else               snprintf(chStr, sizeof(chStr), "CH: %u", (unsigned)chaff);
  if (flare == 0xFF) snprintf(flStr, sizeof(flStr), "FL: --");
  else               snprintf(flStr, sizeof(flStr), "FL: %u", (unsigned)flare);

  int asc = u8g2.getAscent();
  int yBox = 32 - asc;  // top of character cells

  // Helper: draw text, inversed if `inv` is true
  auto drawItem = [&](int x, const char* s, bool inv) {
    int w = u8g2.getStrWidth(s);
    if (inv) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(x, yBox, w, asc);
      u8g2.setFontMode(1);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x, 32, s);
      u8g2.setDrawColor(1);
      u8g2.setFontMode(0);
    } else {
      u8g2.drawStr(x, 32, s);
    }
  };

  // CH: left-aligned
  bool chInv = (chaff != 0xFF && chaff <= 10);
  drawItem(0, chStr, chInv);

  // FL: centered
  int flW = u8g2.getStrWidth(flStr);
  bool flInv = (flare != 0xFF && flare <= 10);
  drawItem((128 - flW) / 2, flStr, flInv);

  // JAMMING: right-justified, only when ECM is transmitting (always inverse)
  if (ecmTx) {
    const char* jmrStr = "JAMMING";
    int jw = u8g2.getStrWidth(jmrStr);
    drawItem(128 - jw, jmrStr, true);
  }

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

void UI::showNoCredentials() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  int w1 = u8g2.getStrWidth("No WiFi Setup");
  int w2 = u8g2.getStrWidth("Press to cont.");
  u8g2.drawStr((128 - w1) / 2,  8, "No WiFi Setup");
  u8g2.drawStr((128 - w2) / 2, 32, "Press to cont.");
  u8g2.sendBuffer();
}

void UI::showBleActive(bool connected) {
  const char* line2 = connected ? "--== CONNECTED ==--" : "--== WAITING ==--";
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  int w1 = u8g2.getStrWidth("AN/BRU-370 BLUETOOTH");
  int w2 = u8g2.getStrWidth(line2);
  int w4 = u8g2.getStrWidth("LP to Cancel");
  u8g2.drawStr((128 - w1) / 2,  8, "AN/BRU-370 BLUETOOTH");
  u8g2.drawStr((128 - w2) / 2, 16, line2);
  u8g2.drawStr((128 - w4) / 2, 32, "LP to Cancel");
  u8g2.sendBuffer();
}

void UI::showWifiMenu(int sel, int rssi, const char* ssid, const char* ip,
                      bool wifiEnabled, uint8_t gStatus) {
  const char* toggleLabel = wifiEnabled ? "DISABLE" : "ENABLE";
  const char* kItems[] = { toggleLabel, "Secrets", "Connect", "Back" };
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel (x=0..63)
  char dbmLine[13], ssidLine[13], ipLine[14];
  if (rssi < 0) snprintf(dbmLine, sizeof(dbmLine), "WiFi %ddBm", rssi);
  else          strlcpy(dbmLine, "WiFi ----", sizeof(dbmLine));

  snprintf(ssidLine, sizeof(ssidLine), "S:%.9s", (ssid && ssid[0]) ? ssid : "----");

  const char* after2 = (ip && ip[0]) ? ip : "";
  int dots = 0;
  for (const char* p = after2; *p; p++) {
    if (*p == '.' && ++dots == 2) { after2 = p + 1; break; }
  }
  if (dots < 2) after2 = "-.--";
  snprintf(ipLine, sizeof(ipLine), "I:x.x.%s", after2);

  const char* gStr = (gStatus == 0) ? "G:..." :
                     (gStatus == 1) ? "G:Online" : "G:Offline";

  u8g2.drawStr(0,  8, dbmLine);
  u8g2.drawStr(0, 16, ssidLine);
  u8g2.drawStr(0, 24, ipLine);
  u8g2.drawStr(0, 32, gStr);

  // Right panel (x=65+)
  for (int i = 0; i < 4; i++) {
    int y = 8 + i * 8;
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, kItems[i]); }
    else          { u8g2.drawStr(71, y, kItems[i]); }
  }
  u8g2.sendBuffer();
}

void UI::showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus) {
  static const char* kItems[] = { "Zeroize", "BLE TERM", "Scan", "Back" };
  const char* passStr = (passStatus == 0) ? "P: ----" :
                        (passStatus == 1) ? "P: NONE" : "P: ****";
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel
  char ssidLine[13];
  snprintf(ssidLine, sizeof(ssidLine), "S:%.9s", (savedSSID && savedSSID[0]) ? savedSSID : "----");
  u8g2.drawStr(0,  8, "WiFi SECRETS");
  u8g2.drawStr(0, 24, ssidLine);
  u8g2.drawStr(0, 32, passStr);

  // Right panel
  for (int i = 0; i < 4; i++) {
    int y = 8 + i * 8;
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, kItems[i]); }
    else          { u8g2.drawStr(71, y, kItems[i]); }
  }
  u8g2.sendBuffer();
}

void UI::showNotImplemented() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "NOT IMPLEMENTED");
  u8g2.drawStr(0, 16, "SP = Back");
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
