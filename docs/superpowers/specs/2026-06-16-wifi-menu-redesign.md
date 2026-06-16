# WiFi Menu Redesign — Zeroize + Live Status

**Version target:** v0.29
**Status:** Ready for implementation

---

## Overview

Three changes bundled as one cohesive redesign:

1. **WiFi menu** — new live status left panel (dBm, SSID, IP, internet check) + simplified 4-item right menu
2. **Secrets submenu** — new screen showing saved credential status with Zeroize, BLE TERM, Manual stub
3. **Cleanup** — remove BLE confirm dialog, remove manual encoder entry, remove dead functions

---

## Screen Layout Reference

OLED is 128×32 px, `u8g2_font_5x7_tr` (5px/char). Left panel is x=0..63 (≤12 chars). Right panel starts at x=65 (cursor `>` at x=65, label at x=71).

---

## 1. WiFi Menu (`WIFI_MENU` state)

Replaces current `showWifiSubMenu()`.

### Left panel (status)

```
WiFi -56dBm      ← y=8   dBm from WiFi.RSSI(); "WiFi ----" if disconnected
S: HomeNetwo     ← y=16  "S: " + activeSSID() truncated to 9 chars; "S: ----" if none
I:x.x.1.105     ← y=24  last 2 IP octets (existing after-2nd-dot logic); "I:x.x.-.--" if no IP
G:Online         ← y=32  internet check result; "G:Offline" or "G:..." while pending
```

**dBm format:** `snprintf(buf, 12, "WiFi %ddBm", rssi)` when rssi < 0; `"WiFi ----"` otherwise.

**G: Online/Offline check:** `WifiMgr::checkInternet()` — does `WiFi.hostByName("raw.githubusercontent.com", ip)`. Result cached for 30 s. First call blocks up to ~2 s (DNS). Subsequent calls return cached immediately. Show `"G:..."` until first result is available. Only attempt check if `isConnected()` is true; show `"G:Offline"` immediately if not connected.

### Right panel (4 items, no scroll needed)

| Index | Label | Action |
|-------|-------|--------|
| 0 | `Connect` | SP → `reconnect()`, show connecting screen, return to WIFI_MENU |
| 1 | `Secrets` | SP → `s_mode = SECRETS_MENU` |
| 2 | `Enabled` / `Disabled` | SP → toggle NVS wifi_en, restart |
| 3 | `Back` | SP → `s_mode = SETTINGS` |

LP anywhere → `s_mode = SETTINGS`.

---

## 2. Secrets Menu (`SECRETS_MENU` state — new)

New `MenuState` enum value. Add after `WIFI_MENU`.

### Left panel (info)

```
WiFi Secrets.    ← y=8   static header (13 chars, 65 px — within left panel)
                 ← y=16  blank
S: HomeNetwo     ← y=24  NVS SSID truncated to 9 chars; "S: ----" if not set
P: ****          ← y=32  see passStatus rules below
```

**passStatus rules** (determined at menu entry by reading NVS):
- No credentials at all (NVS ssid == ""): `P: ----`
- Credentials set, password empty (open network): `P: NONE`
- Credentials set, password non-empty: `P: ****`

### Right panel (3 items, no scroll)

| Index | Label | Action |
|-------|-------|--------|
| 0 | `Zeroize` | SP → `clearOverride()` + `ESP.restart()` |
| 1 | `BLE TERM` | SP → `runBleSetup(...)` directly (no confirmation dialog) |
| 2 | `Manual` | SP → `showNotImplemented()`, wait for SP, return to SECRETS_MENU |

LP anywhere → `s_mode = WIFI_MENU`.

---

## 3. New WifiMgr Functions

### `int WifiMgr::rssi()`

```cpp
int WifiMgr::rssi() { return WiFi.RSSI(); }
```

Returns 0 or positive when disconnected. Callers show `"----"` for any value ≥ 0.

### `bool WifiMgr::checkInternet()`

```cpp
bool WifiMgr::checkInternet() {
  static bool s_result = false;
  static unsigned long s_lastCheck = 0;
  static bool s_hasResult = false;
  if (!isConnected()) { s_hasResult = false; s_lastCheck = 0; return false; }
  unsigned long now = millis();
  if (s_hasResult && now - s_lastCheck < 30000UL) return s_result;
  IPAddress ip;
  s_result = (WiFi.hostByName("raw.githubusercontent.com", ip) == 1);
  s_lastCheck = now;
  s_hasResult = true;
  return s_result;
}
```

### `void WifiMgr::nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus)`

Reads NVS without affecting `s_ssid`/`s_pass`. passStatus: 0=none, 1=open(NONE), 2=set(****).

```cpp
void WifiMgr::nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus) {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  strlcpy(ssidOut, ssid.c_str(), ssidLen);
  if (ssid.length() == 0)      *passStatus = 0;
  else if (pass.length() == 0) *passStatus = 1;
  else                          *passStatus = 2;
}
```

---

## 4. New / Changed UI Functions

### `showWifiMenu(int sel, int rssi, const char* ssid, const char* ip, bool wifiEnabled, uint8_t gStatus)`

Replaces `showWifiSubMenu()`. `gStatus`: 0=pending(`...`), 1=online, 2=offline.

```cpp
void UI::showWifiMenu(int sel, int rssi, const char* ssid, const char* ip,
                      bool wifiEnabled, uint8_t gStatus) {
  static const char* items[] = { "Connect", "Secrets",
                                  nullptr,    // Enabled/Disabled — computed below
                                  "Back" };
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel
  char dbmLine[13], ssidLine[13], ipLine[13], gLine[12];
  if (rssi < 0) snprintf(dbmLine,  sizeof(dbmLine),  "WiFi %ddBm", rssi);
  else          strlcpy(dbmLine,  "WiFi ----",        sizeof(dbmLine));

  snprintf(ssidLine, sizeof(ssidLine), "S:%.9s", (ssid && ssid[0]) ? ssid : "----");

  const char* after2 = ip ? ip : "";
  int dots = 0;
  for (const char* p = after2; *p; p++) {
    if (*p == '.' && ++dots == 2) { after2 = p + 1; break; }
  }
  if (dots < 2) after2 = "-.--";
  snprintf(ipLine, sizeof(ipLine), "I:x.x.%s", after2);

  const char* gStr = (gStatus == 0) ? "G:..." :
                     (gStatus == 1) ? "G:Online" : "G:Offline";
  strlcpy(gLine, gStr, sizeof(gLine));

  u8g2.drawStr(0,  8, dbmLine);
  u8g2.drawStr(0, 16, ssidLine);
  u8g2.drawStr(0, 24, ipLine);
  u8g2.drawStr(0, 32, gLine);

  // Right panel
  const char* toggleLabel = wifiEnabled ? "Enabled" : "Disabled";
  for (int i = 0; i < 4; i++) {
    int y = 8 + i * 8;
    const char* label = (i == 2) ? toggleLabel : items[i];
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, label); }
    else          { u8g2.drawStr(71, y, label); }
  }
  u8g2.sendBuffer();
}
```

### `showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus)`

```cpp
void UI::showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus) {
  static const char* items[] = { "Zeroize", "BLE TERM", "Manual" };
  const char* passStr = (passStatus == 0) ? "P: ----" :
                        (passStatus == 1) ? "P: NONE" : "P: ****";
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel
  char ssidLine[13];
  snprintf(ssidLine, sizeof(ssidLine), "S:%.9s", (savedSSID && savedSSID[0]) ? savedSSID : "----");
  u8g2.drawStr(0,  8, "WiFi Secrets.");
  u8g2.drawStr(0, 24, ssidLine);
  u8g2.drawStr(0, 32, passStr);

  // Right panel
  for (int i = 0; i < 3; i++) {
    int y = 8 + i * 8;
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, items[i]); }
    else          { u8g2.drawStr(71, y, items[i]); }
  }
  u8g2.sendBuffer();
}
```

### `showNotImplemented()`

```cpp
void UI::showNotImplemented() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "NOT IMPLEMENTED");
  u8g2.drawStr(0, 16, "SP = Back");
  u8g2.sendBuffer();
}
```

---

## 5. main.cpp Changes

### MenuState enum

Add `SECRETS_MENU` after `WIFI_MENU`:
```cpp
MOUSE_TUNE_MENU, WIFI_MENU, SECRETS_MENU,
```

### State variables for WIFI_MENU

Add or retain:
```cpp
static int  s_wifiSubSel    = 0;   // shared between WIFI_MENU and SECRETS_MENU
static uint8_t s_gStatus    = 0;   // 0=pending, 1=online, 2=offline
static bool s_gChecked      = false;
```

### WIFI_MENU handler (replace existing)

```cpp
} else if (s_mode == WIFI_MENU) {
  int delta = Encoder::readDelta();
  s_wifiSubSel = (s_wifiSubSel + delta + 4) % 4;

  if (Encoder::shortPressed()) {
    switch (s_wifiSubSel) {
      case 0:  // Connect
        WifiMgr::reconnect();
        { unsigned long t0 = millis(); bool ok = false;
          while (millis() - t0 < 15000UL) {
            UI::showWifiConnecting(WifiMgr::activeSSID());
            if (WifiMgr::pollConnect()) { ok = true; break; }
            delay(100);
          }
          if (ok) {
            UI::showWifiConnected(WifiMgr::activeSSID());
            if (!s_dcsBiosStarted) {
              DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                             "255.255.255.255", DCSBIOS_CMD_PORT);
              s_dcsBiosStarted = true;
            }
          } else {
            UI::showWifiFailed(WifiMgr::activeSSID());
            delay(1500);
          }
        }
        s_gStatus = 0; s_gChecked = false;  // re-check internet after reconnect
        break;
      case 1:  // Secrets
        s_wifiSubSel = 0;
        s_mode = SECRETS_MENU;
        break;
      case 2:  // Enabled/Disabled toggle
        s_wifiEnabled = !s_wifiEnabled;
        { Preferences p; p.begin("brew", false); p.putInt("wifi_en", s_wifiEnabled ? 1 : 0); p.end(); }
        UI::showSaved();
        ESP.restart();
        break;
      case 3:  // Back
        s_wifiSubSel = 0;
        s_mode = SETTINGS;
        break;
    }
  }
  if (Encoder::longPressed()) { s_wifiSubSel = 0; s_mode = SETTINGS; }
```

### SECRETS_MENU handler (new)

```cpp
} else if (s_mode == SECRETS_MENU) {
  int delta = Encoder::readDelta();
  s_wifiSubSel = (s_wifiSubSel + delta + 3) % 3;

  if (Encoder::shortPressed()) {
    switch (s_wifiSubSel) {
      case 0:  // Zeroize
        WifiMgr::clearOverride();
        UI::showSaved();
        ESP.restart();
        break;
      case 1:  // BLE TERM
        { bool saved = WifiMgr::runBleSetup(
            []() { UI::showBleActive(WifiMgr::isBleClientConnected()); },
            []() { Encoder::readDelta(); return Encoder::longPressed(); });
          if (saved) ESP.restart();
        }
        s_wifiSubSel = 0;
        s_mode = SECRETS_MENU;
        break;
      case 2:  // Manual — stub
        UI::showNotImplemented();
        while (!Encoder::shortPressed()) { Encoder::readDelta(); delay(10); }
        break;
    }
  }
  if (Encoder::longPressed()) { s_wifiSubSel = 0; s_mode = WIFI_MENU; }
```

### Internet check — trigger on WIFI_MENU entry

In the `s_mode = WIFI_MENU` transition (wherever it's set), reset the check:
```cpp
s_gStatus = 0; s_gChecked = false;
```

In the OLED update `case WIFI_MENU:`:
```cpp
case WIFI_MENU: {
  if (!s_gChecked) {
    bool ok = WifiMgr::checkInternet();  // blocks ~2s first call only
    s_gStatus = ok ? 1 : 2;
    s_gChecked = true;
  }
  UI::showWifiMenu(s_wifiSubSel,
                   WifiMgr::rssi(),
                   WifiMgr::activeSSID(),
                   WifiMgr::activeIP(),
                   s_wifiEnabled,
                   s_gStatus);
  break;
}
```

### SECRETS_MENU OLED update

```cpp
case SECRETS_MENU: {
  char nvsSsid[64] = {0};
  uint8_t passStatus = 0;
  WifiMgr::nvsCredentials(nvsSsid, sizeof(nvsSsid), &passStatus);
  UI::showSecretsMenu(s_wifiSubSel, nvsSsid, passStatus);
  break;
}
```

Note: `nvsCredentials()` is called every OLED tick (~20 Hz). It opens NVS read-only each call. If this causes I2C/NVS contention, cache the values on SECRETS_MENU entry instead.

---

## 6. Cleanup (remove dead code)

### Remove from `include/ui.h` and `src/ui.cpp`:
- `showWifiSubMenu()` — replaced by `showWifiMenu()`
- `showWifiConfirm()` — BLE no longer needs a confirm dialog
- `showCharEntry()` — manual entry removed

### Remove from `include/wifi_mgr.h` and `src/wifi_mgr.cpp`:
- `runEncoderEntry()` — manual entry removed
- `runSerialSetup()` — not called anywhere (verify before removing)

### Remove from `src/main.cpp`:
- Old WIFI_MENU handler block
- `showWifiConfirm()` confirm loop before BLE launch
- All `runEncoderEntry()` call sites

---

## 7. Summary of File Changes

| File | Change |
|------|--------|
| `include/wifi_mgr.h` | Add `rssi()`, `checkInternet()`, `nvsCredentials()` |
| `src/wifi_mgr.cpp` | Add `rssi()`, `checkInternet()`, `nvsCredentials()` implementations; remove `runEncoderEntry()`, `runSerialSetup()` |
| `include/ui.h` | Add `showWifiMenu()`, `showSecretsMenu()`, `showNotImplemented()`; remove `showWifiSubMenu()`, `showWifiConfirm()`, `showCharEntry()` |
| `src/ui.cpp` | Implement new; remove old |
| `src/main.cpp` | Add `SECRETS_MENU` enum; rewrite WIFI_MENU handler; add SECRETS_MENU handler; add OLED cases; add `s_gStatus`/`s_gChecked` |

---

## 8. Testing

1. WiFi connected — left panel shows live dBm, SSID, IP, and `G:Online` after brief pause
2. WiFi disconnected — left panel shows `WiFi ----`, `S: ----`, `I:x.x.-.--`, `G:Offline`
3. Connect item — reinitializes connection, shows connecting screen
4. Enabled/Disabled toggle — disables WiFi, restarts, on boot WiFi is off
5. Secrets menu — shows current NVS SSID and P: status (`----`/`NONE`/`****`)
6. Zeroize — wipes credentials, reboots to "No WiFi Setup" screen
7. BLE TERM — launches BLE directly (no confirm dialog), banner appears in Bluefruit
8. Manual — shows "NOT IMPLEMENTED / SP = Back"
9. LP from Secrets → returns to WiFi menu; LP from WiFi menu → returns to Settings
