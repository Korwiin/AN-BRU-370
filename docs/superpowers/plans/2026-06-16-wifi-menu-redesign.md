# WiFi Menu Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the WiFi submenu with a live-status left panel (dBm, SSID, IP, internet check) and a redesigned right menu (Connect / Secrets / Enabled / Back), plus a new Secrets submenu with Zeroize, BLE TERM, and Manual stub.

**Architecture:** Four sequential tasks. Tasks 1 and 2 add new functions without removing old ones (so the build stays green). Task 3 rewrites main.cpp to use the new API. Task 4 removes dead code and bumps the version.

**Tech Stack:** ESP32-S3 / PlatformIO / Arduino; u8g2 OLED; Preferences (NVS); WiFi (DNS lookup for internet check)

**Spec:** `docs/superpowers/specs/2026-06-16-wifi-menu-redesign.md`

---

## File Map

| File | Change |
|------|--------|
| `include/wifi_mgr.h` | Add `rssi()`, `checkInternet()`, `nvsCredentials()` declarations; Task 4 removes `runEncoderEntry()`, `runSerialSetup()` |
| `src/wifi_mgr.cpp` | Add three implementations; Task 4 removes two |
| `include/ui.h` | Add `showWifiMenu()`, `showSecretsMenu()`, `showNotImplemented()`; Task 4 removes `showWifiSubMenu()`, `showWifiConfirm()`, `showCharEntry()` |
| `src/ui.cpp` | Add three implementations; Task 4 removes three |
| `src/main.cpp` | Task 3: add `SECRETS_MENU` to enum; rewrite WIFI_MENU handler; add SECRETS_MENU handler; add OLED cases; add `s_gStatus`/`s_gChecked`/`s_nvsSsid`/`s_nvsPassStatus` statics |
| `include/config.h.example` + `include/config.h` | Task 4: bump to v0.29 |

---

## Task 1: New WifiMgr Functions

**Files:**
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`

### Context

Three new functions. Do not remove any existing functions yet — that happens in Task 4.

`checkInternet()` calls `WiFi.hostByName()` which blocks ~100–2000 ms on first call (DNS). Result is cached for 30 s. Only runs if `isConnected()` is true.

`nvsCredentials()` reads from NVS namespace `"brew_wifi"`, keys `"ssid"` and `"pass"`. Same namespace used by `saveCredentials()` and `clearOverride()`.

passStatus values: `0` = no ssid set (`----`), `1` = ssid set but pass empty — open network (`NONE`), `2` = ssid and pass both set (`****`).

- [ ] **Step 1: Add declarations to `include/wifi_mgr.h`**

After the existing `bool isBleClientConnected();` line, add:

```cpp
int  rssi();
bool checkInternet();
void nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus);
```

- [ ] **Step 2: Add `rssi()` implementation to `src/wifi_mgr.cpp`**

Add after `isBleClientConnected()`:

```cpp
int WifiMgr::rssi() { return WiFi.RSSI(); }
```

- [ ] **Step 3: Add `checkInternet()` implementation**

Add after `rssi()`:

```cpp
bool WifiMgr::checkInternet() {
  static bool          s_result   = false;
  static unsigned long s_lastCheck = 0;
  static bool          s_hasResult = false;
  if (!isConnected()) { s_hasResult = false; s_lastCheck = 0; return false; }
  unsigned long now = millis();
  if (s_hasResult && now - s_lastCheck < 30000UL) return s_result;
  IPAddress ip;
  s_result   = (WiFi.hostByName("raw.githubusercontent.com", ip) == 1);
  s_lastCheck = now;
  s_hasResult = true;
  return s_result;
}
```

- [ ] **Step 4: Add `nvsCredentials()` implementation**

Add after `checkInternet()`:

```cpp
void WifiMgr::nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus) {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  strlcpy(ssidOut, ssid.c_str(), ssidLen);
  if      (ssid.length() == 0) *passStatus = 0;
  else if (pass.length() == 0) *passStatus = 1;
  else                          *passStatus = 2;
}
```

- [ ] **Step 5: Compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -5
```

Expected: SUCCESS, no errors.

- [ ] **Step 6: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat(wifi): add rssi(), checkInternet(), nvsCredentials()"
```

---

## Task 2: New UI Functions

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

### Context

Three new functions. Do not remove `showWifiSubMenu()`, `showWifiConfirm()`, or `showCharEntry()` yet — removal is in Task 4.

OLED is 128×32 px, `u8g2_font_5x7_tr` (5 px/char wide). Left panel: x=0..63 (≤12 chars). Right panel: cursor `>` at x=65, label at x=71.

`showWifiMenu()` has 4 right-panel items (no scrolling). `showSecretsMenu()` has 3 right-panel items (no scrolling).

- [ ] **Step 1: Add declarations to `include/ui.h`**

Add after the existing `showWifiSubMenu()` declaration:

```cpp
void showWifiMenu(int sel, int rssi, const char* ssid, const char* ip,
                  bool wifiEnabled, uint8_t gStatus);
void showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus);
void showNotImplemented();
```

- [ ] **Step 2: Implement `showWifiMenu()` in `src/ui.cpp`**

Add before or after `showWifiSubMenu()`:

```cpp
void UI::showWifiMenu(int sel, int rssi, const char* ssid, const char* ip,
                      bool wifiEnabled, uint8_t gStatus) {
  static const char* kItems[] = { "Connect", "Secrets", nullptr, "Back" };
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel (x=0..63)
  char dbmLine[13], ssidLine[13], ipLine[13];
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
  const char* toggleLabel = wifiEnabled ? "Enabled" : "Disabled";
  for (int i = 0; i < 4; i++) {
    int y = 8 + i * 8;
    const char* label = (i == 2) ? toggleLabel : kItems[i];
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, label); }
    else          { u8g2.drawStr(71, y, label); }
  }
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Implement `showSecretsMenu()` in `src/ui.cpp`**

Add after `showWifiMenu()`:

```cpp
void UI::showSecretsMenu(int sel, const char* savedSSID, uint8_t passStatus) {
  static const char* kItems[] = { "Zeroize", "BLE TERM", "Manual" };
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
    if (i == sel) { u8g2.drawStr(65, y, ">"); u8g2.drawStr(71, y, kItems[i]); }
    else          { u8g2.drawStr(71, y, kItems[i]); }
  }
  u8g2.sendBuffer();
}
```

- [ ] **Step 4: Implement `showNotImplemented()` in `src/ui.cpp`**

Add after `showSecretsMenu()`:

```cpp
void UI::showNotImplemented() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "NOT IMPLEMENTED");
  u8g2.drawStr(0, 16, "SP = Back");
  u8g2.sendBuffer();
}
```

- [ ] **Step 5: Compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -5
```

Expected: SUCCESS. Old functions still present, no references broken.

- [ ] **Step 6: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat(ui): add showWifiMenu(), showSecretsMenu(), showNotImplemented()"
```

---

## Task 3: main.cpp — New Menu Handlers

**Files:**
- Modify: `src/main.cpp`

### Context

Read `src/main.cpp` in full before making any changes. Key areas to understand:

1. **`MenuState` enum** (near top of file) — add `SECRETS_MENU` after `WIFI_MENU`
2. **Static state vars** (near top of file, around `s_wifiSubSel`) — add `s_gStatus`, `s_gChecked`, `s_nvsSsid[64]`, `s_nvsPassStatus`
3. **`s_mode = WIFI_MENU` transition sites** — every place that sets `s_mode = WIFI_MENU` must also reset `s_gStatus = 0; s_gChecked = false; s_wifiSubSel = 0;`
4. **Old WIFI_MENU handler** (`} else if (s_mode == WIFI_MENU)`) — replace entirely
5. **Old `case WIFI_MENU:` in the OLED update switch** — replace
6. **OLED update switch** — add `case SECRETS_MENU:`

`s_wifiSubSel` is shared between `WIFI_MENU` (4 items, modulo 4) and `SECRETS_MENU` (3 items, modulo 3). Reset it to 0 on every state transition into either menu.

`s_nvsSsid` and `s_nvsPassStatus` are read once on `SECRETS_MENU` entry (not every tick) to avoid calling NVS at 20 Hz.

- [ ] **Step 1: Add `SECRETS_MENU` to the `MenuState` enum**

Find the enum (look for `WIFI_MENU`). Add `SECRETS_MENU` immediately after `WIFI_MENU`:

```cpp
MOUSE_TUNE_MENU, WIFI_MENU, SECRETS_MENU,
```

- [ ] **Step 2: Add new static state variables**

Near `s_wifiSubSel` and `s_wifiSubOffset`, add:

```cpp
static uint8_t s_gStatus       = 0;   // 0=pending, 1=online, 2=offline
static bool    s_gChecked      = false;
static char    s_nvsSsid[64]   = {0};
static uint8_t s_nvsPassStatus = 0;
```

Also keep `s_wifiSubSel`. Remove `s_wifiSubOffset` (no longer needed — menus don't scroll). Search for all uses of `s_wifiSubOffset` and remove them.

- [ ] **Step 3: Reset state on every WIFI_MENU transition**

Search for all sites that set `s_mode = WIFI_MENU` (there will be several — from SETTINGS long-press, from SECRETS_MENU LP, from Connect completion, etc.). At each one, add:

```cpp
s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
s_mode = WIFI_MENU;
```

- [ ] **Step 4: Reset state on SECRETS_MENU entry**

Wherever `s_mode = SECRETS_MENU` is set (only in the new Secrets handler below), also populate NVS cache:

```cpp
s_wifiSubSel = 0;
WifiMgr::nvsCredentials(s_nvsSsid, sizeof(s_nvsSsid), &s_nvsPassStatus);
s_mode = SECRETS_MENU;
```

- [ ] **Step 5: Replace old WIFI_MENU input handler**

Find the old `} else if (s_mode == WIFI_MENU) {` block and replace it entirely with:

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
        s_gStatus = 0; s_gChecked = false;  // re-check after reconnect attempt
        break;
      case 1:  // Secrets
        s_wifiSubSel = 0;
        WifiMgr::nvsCredentials(s_nvsSsid, sizeof(s_nvsSsid), &s_nvsPassStatus);
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

- [ ] **Step 6: Add SECRETS_MENU input handler**

Add the SECRETS_MENU handler immediately after the WIFI_MENU handler:

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
      case 1:  // BLE TERM — launch directly, no confirm dialog
        { bool saved = WifiMgr::runBleSetup(
            []() { UI::showBleActive(WifiMgr::isBleClientConnected()); },
            []() { Encoder::readDelta(); return Encoder::longPressed(); });
          if (saved) ESP.restart();
        }
        s_wifiSubSel = 0;
        WifiMgr::nvsCredentials(s_nvsSsid, sizeof(s_nvsSsid), &s_nvsPassStatus);
        s_mode = SECRETS_MENU;
        break;
      case 2:  // Manual — stub
        UI::showNotImplemented();
        Encoder::flush();
        while (!Encoder::shortPressed()) { Encoder::readDelta(); delay(10); }
        break;
    }
  }
  if (Encoder::longPressed()) {
    s_wifiSubSel = 0; s_gStatus = 0; s_gChecked = false;
    s_mode = WIFI_MENU;
  }
```

- [ ] **Step 7: Update OLED switch — replace `case WIFI_MENU:` and add `case SECRETS_MENU:`**

Find the OLED update switch statement. Replace the old `case WIFI_MENU:` with:

```cpp
case WIFI_MENU:
  if (!s_gChecked) {
    s_gStatus  = WifiMgr::checkInternet() ? 1 : 2;
    s_gChecked = true;
  }
  UI::showWifiMenu(s_wifiSubSel,
                   WifiMgr::rssi(),
                   WifiMgr::activeSSID(),
                   WifiMgr::activeIP(),
                   s_wifiEnabled,
                   s_gStatus);
  break;
```

Add immediately after:

```cpp
case SECRETS_MENU:
  UI::showSecretsMenu(s_wifiSubSel, s_nvsSsid, s_nvsPassStatus);
  break;
```

- [ ] **Step 8: Compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -10
```

Expected: SUCCESS. Old functions still present in wifi_mgr and ui (not yet removed), so no linker errors if any old call sites were missed. Fix any errors before continuing.

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp
git commit -m "feat: new WiFi menu with live status panel and Secrets submenu"
```

---

## Task 4: Dead Code Removal + Version Bump

**Files:**
- Modify: `include/wifi_mgr.h`, `src/wifi_mgr.cpp`
- Modify: `include/ui.h`, `src/ui.cpp`
- Modify: `include/config.h.example`, `include/config.h`

### Context

Remove functions that are no longer called anywhere. Verify before deleting:

- `runEncoderEntry()` — called in old WIFI_MENU handler (now removed from main.cpp). Verify: `grep -rn "runEncoderEntry" src/`
- `runSerialSetup()` — was never called from main.cpp (confirmed). Verify: `grep -rn "runSerialSetup" src/`
- `showWifiSubMenu()` — replaced by `showWifiMenu()`. Verify: `grep -rn "showWifiSubMenu" src/`
- `showWifiConfirm()` — old BLE confirm dialog (removed from main.cpp). Verify: `grep -rn "showWifiConfirm" src/`
- `showCharEntry()` — used by `runEncoderEntry()` call sites (all removed). Verify: `grep -rn "showCharEntry" src/`
- `s_wifiSubOffset` — no longer used after Task 3. Verify: `grep -rn "wifiSubOffset" src/`

Only delete a function after confirming zero references in `src/`.

- [ ] **Step 1: Verify dead functions**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
grep -rn "runEncoderEntry\|runSerialSetup\|showWifiSubMenu\|showWifiConfirm\|showCharEntry\|wifiSubOffset" src/
```

Expected: zero matches. If any remain, find them and remove the call site first.

- [ ] **Step 2: Remove from `include/wifi_mgr.h`**

Delete the `runEncoderEntry()` declaration (multi-line, ~6 lines).
Delete the `runSerialSetup()` declaration.

- [ ] **Step 3: Remove from `src/wifi_mgr.cpp`**

Delete the `runEncoderEntry()` implementation body.
Delete the `runSerialSetup()` implementation body.

- [ ] **Step 4: Remove from `include/ui.h`**

Delete declarations:
- `void showWifiSubMenu(...);`
- `void showWifiConfirm();`
- `void showCharEntry(...);`

- [ ] **Step 5: Remove from `src/ui.cpp`**

Delete implementations:
- `UI::showWifiSubMenu()`
- `UI::showWifiConfirm()`
- `UI::showCharEntry()`

- [ ] **Step 6: Bump version**

In `include/config.h.example`:
```cpp
#define FIRMWARE_VERSION     "0.29"
#define FIRMWARE_VERSION_BCD  0x0029
```

In `include/config.h` (gitignored):
```cpp
#define FIRMWARE_VERSION     "0.29"
#define FIRMWARE_VERSION_BCD  0x0029
```

- [ ] **Step 7: Final compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -5
```

Expected: SUCCESS, zero errors, zero warnings about unused functions.

- [ ] **Step 8: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp include/ui.h src/ui.cpp \
        include/config.h.example
git commit -m "refactor: remove manual entry, serial setup, old WiFi confirm dialog; bump v0.29"
```

---

## Manual Testing Checklist (after flash)

- [ ] WiFi connected: left panel shows live dBm (negative number), SSID, last-2-octet IP, `G:Online` after ~1s pause
- [ ] WiFi disconnected: shows `WiFi ----`, `S:----`, `I:x.x.-.--`, `G:Offline`
- [ ] Connect item: reinitializes connection, shows connecting/connected/failed screen
- [ ] Enabled → Disabled toggle: disables WiFi, reboots; re-entering menu shows "Disabled"
- [ ] Secrets menu entry: shows header, NVS SSID truncated, correct P: status (`----`/`NONE`/`****`)
- [ ] Zeroize: clears credentials, reboots to "No WiFi Setup / Press to cont." screen
- [ ] BLE TERM: launches BLE directly (no confirm dialog), OLED shows WAITING, Bluefruit shows banner
- [ ] Manual: shows "NOT IMPLEMENTED / SP = Back", SP returns to Secrets menu
- [ ] LP from Secrets → WiFi menu; LP from WiFi → Settings
