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

void UI::showBootStatus(const BootStatusInfo& s) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Line 1 (y=8): device name left, firmware version right
  u8g2.drawStr(0, 8, "AN/BRU-370");
  {
    char ver[10];
    snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
    u8g2.drawStr(128 - u8g2.getStrWidth(ver), 8, ver);
  }

  // Line 2 (y=16): intentional empty gap

  // Line 3 (y=24): 3 indicators — WiFi | IP | DCS
  // Columns shifted +4px each (vs. the original 46/88) to give the WiFi slot
  // room for 4-character "WPA2"/"WPA3" text without colliding with "IP".
  static const char kSpin[] = { '-', '\\', '|', '/' };
  const char spinCh = kSpin[(millis() / 250) % 4];

  const int kGap = 2;
  const int xWL  = 2;
  const int xWS  = xWL + u8g2.getStrWidth("WiFi") + kGap;  // ~24
  const int xIL  = 50;
  const int xIS  = xIL + u8g2.getStrWidth("IP")   + kGap;  // ~62
  const int xDL  = 92;
  const int xDS  = xDL + u8g2.getStrWidth("DCS")  + kGap;  // ~109

  u8g2.drawStr(xWL, 24, "WiFi");
  u8g2.drawStr(xIL, 24, "IP");
  u8g2.drawStr(xDL, 24, "DCS");

  // WiFi slot: spinner until associated, then auth mode text ("WPA2"/"WPA3").
  // No pending "--" state — WiFi is the first phase, always either spinning
  // or done.
  if (s.wifi) {
    u8g2.drawStr(xWS, 24, s.authMode ? s.authMode : "");
  } else {
    char buf[2] = { spinCh, '\0' };
    u8g2.drawStr(xWS, 24, buf);
  }

  // IP slot: pending "--" until WiFi associates, spinner until IP arrives,
  // then the last octet as plain digits (e.g. "42").
  if (s.ip) {
    u8g2.drawStr(xIS, 24, s.ipOctet ? s.ipOctet : "");
  } else if (s.wifi) {
    char buf[2] = { spinCh, '\0' };
    u8g2.drawStr(xIS, 24, buf);
  } else {
    u8g2.drawStr(xIS, 24, "--");
  }

  // DCS slot: pending "--" until IP arrives, then spins forever — no "done"
  // state, no checkmark. The boot screen exits ~1.5s after DCS connects, so
  // a completed-DCS visual is never actually visible; spinning the whole time
  // it could be live is simpler and equally informative.
  if (s.ip) {
    char buf[2] = { spinCh, '\0' };
    u8g2.drawStr(xDS, 24, buf);
  } else {
    u8g2.drawStr(xDS, 24, "--");
  }

  // Line 4 (y=32): exactly one of three static, centered states.
  // Priority: failReason (with live retry countdown) > Connecting > Waiting for DCS.
  char line4[32] = {0};
  if (s.failReason) {
    snprintf(line4, sizeof(line4), "%s Retry %d", s.failReason, s.retrySecs);
  } else if (!s.ip) {
    snprintf(line4, sizeof(line4), "Connecting ...");
  } else if (!s.dcs) {
    snprintf(line4, sizeof(line4), "Waiting for DCS ...");
  }
  if (line4[0]) {
    int w = u8g2.getStrWidth(line4);
    u8g2.drawStr((128 - w) / 2, 32, line4);
  }

  u8g2.sendBuffer();
}


void UI::sleep() { u8g2.setPowerSave(1); }
void UI::wake()  { u8g2.setPowerSave(0); }

void UI::showWifiConnecting(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "WIFI CONNECTING");
  u8g2.drawStr(0, 22, ssid);
  u8g2.sendBuffer();
}

void UI::showWifiFailed(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "WIFI FAILED");
  u8g2.drawStr(0, 22, ssid);
  u8g2.sendBuffer();
}

void UI::showWifiConnected(const char* ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
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
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth("WARN");
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, "WARN");
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
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth("MSL L");
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, "MSL L");
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
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth("CONFIG");
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, "CONFIG");
  if (flashState) u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void UI::showNotReady(bool flashState) {
  u8g2.clearBuffer();
  if (flashState) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 32);
    u8g2.setDrawColor(0);
  }
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth("START");
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, "START");
  if (flashState) u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void UI::showSetupRunning(uint8_t step, bool blinkOn) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Line 1: "SETUP X/5" centered
  char hdr[12];
  snprintf(hdr, sizeof(hdr), "SETUP %u/5", (unsigned)step);
  u8g2.drawStr((128 - u8g2.getStrWidth(hdr)) / 2, 8, hdr);

  // Line 3: accumulating labels — current step blinks, completed steps solid
  // X positions built by accumulating label+gap widths
  int xHad  = 0;
  int xTgp  = xHad  + u8g2.getStrWidth("HAD  ");
  int xCmds = xTgp  + u8g2.getStrWidth("TGP  ");
  int xRwr  = xCmds + u8g2.getStrWidth("CMDS  ");

  // A label is visible when: (a) its step is past (solid) or (b) it is the
  // current step and blinkOn is true
  if ((step > 1) || (step == 1 && blinkOn)) u8g2.drawStr(xHad,  24, "HAD");
  if ((step > 2) || (step == 2 && blinkOn)) u8g2.drawStr(xTgp,  24, "TGP");
  if ((step > 3) || (step == 3 && blinkOn)) u8g2.drawStr(xCmds, 24, "CMDS");
  if ((step > 4) || (step == 4 && blinkOn)) u8g2.drawStr(xRwr,  24, "RWR");

  // Line 4: "MWS" blinks on step 5 only
  if (step == 5 && blinkOn) u8g2.drawStr(0, 32, "MWS");

  u8g2.sendBuffer();
}

void UI::showAircraftStatus(uint32_t fuelLbs,
                            const char* chaff, const char* flare, bool ecmTx) {
  u8g2.clearBuffer();

  // --- Fuel number, large font, top half ---
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  char fuelStr[8];
  if (fuelLbs >= 1000)
    snprintf(fuelStr, sizeof(fuelStr), "%u,%03u", fuelLbs / 1000, fuelLbs % 1000);
  else
    snprintf(fuelStr, sizeof(fuelStr), "%u", (unsigned)fuelLbs);
  u8g2.drawStr((128 - u8g2.getStrWidth(fuelStr)) / 2, 25, fuelStr);

  u8g2.setFont(u8g2_font_5x7_tr);

  // --- Bottom zone: CH / FL / JAMMING row ---
  bool blinkOn = (millis() / 250) % 2 == 0;

  // CH:XXXX — raw 4-char DCS string; leading spaces provide natural gap between label and digits
  bool chLow = (chaff[0] == 'L' && chaff[1] == 'o');
  int chLabelW = u8g2.getStrWidth("CH:");
  u8g2.drawStr(0, 32, "CH:");
  if (!chLow || blinkOn)
    u8g2.drawStr(chLabelW, 32, chaff);

  // FL:XXXX — centered as fixed 7-char block; width anchored to "FL:    " so label never moves
  bool flLow = (flare[0] == 'L' && flare[1] == 'o');
  int flBlockW = u8g2.getStrWidth("FL:    ");
  int flLabelW = u8g2.getStrWidth("FL:");
  int flX = (128 - flBlockW) / 2;
  u8g2.drawStr(flX, 32, "FL:");
  if (!flLow || blinkOn)
    u8g2.drawStr(flX + flLabelW, 32, flare);

  // JAMMING: right-justified, always blinks when ECM transmitting
  if (ecmTx && blinkOn) {
    const char* jmrStr = "JAMMING";
    int jw = u8g2.getStrWidth(jmrStr);
    u8g2.drawStr(128 - jw, 32, jmrStr);
  }

  u8g2.sendBuffer();
}

void UI::showMacroMenu(int idx) {
  Macro* m = &macros[idx];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth(m->name);
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, m->name);
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
  u8g2.setFont(u8g2_font_5x7_tr);
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
  u8g2.setFont(u8g2_font_5x7_tr);
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
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(59, 20, "x");
  for (int d = 0; d < 8; d++) {
    int x = kDx[d];
    char dc[2] = {(char)('0' + digits[d]), 0};
    if (d == digitPos) {
      u8g2.setDrawColor(1); u8g2.drawBox(x - 1, 13, 8, 8);
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
  u8g2.setFont(u8g2_font_5x7_tr);
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
                      uint8_t gStatus) {
  const char* kItems[] = { "Secrets", "Connect", "Back" };
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
  for (int i = 0; i < 3; i++) {
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
  u8g2.setFont(u8g2_font_spleen16x32_mr);
  int w = u8g2.getStrWidth("SAVED");
  u8g2.drawStr((128 - w) / 2, (32 + u8g2.getAscent()) / 2, "SAVED");
  u8g2.sendBuffer();
  delay(600);
}

void UI::showRebootCountdown(int secs) {
  char buf[20];
  snprintf(buf, sizeof(buf), "Rebooting in %ds", secs);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(buf))      / 2, 12, buf);
  u8g2.drawStr((128 - u8g2.getStrWidth("SP=Cancel")) / 2, 27, "SP=Cancel");
  u8g2.sendBuffer();
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
