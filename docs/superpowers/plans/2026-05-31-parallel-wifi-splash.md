# Parallel WiFi + Splash Progress Bar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Start WiFi in parallel with the splash screen, show a 1px growing progress bar at the bottom row, confirm connection with a brief "WiFi Connected" message, and support long-press cancel for HID-only mode.

**Architecture:** Split `WifiMgr::begin()` into non-blocking `startConnect()` / `pollConnect()` / `cancelConnect()`. Add `UI::showSplashProgress()` that redraws the splash each tick with status. Refactor `setup()` to run the splash+WiFi loop in parallel, and add a background connect check in `loop()` for the case where the splash was dismissed before WiFi connected.

**Tech Stack:** C++, Arduino ESP32, U8g2, PlatformIO (`esp32s3_supermini` env)

---

## File Map

- Modify: `include/wifi_mgr.h` — add `startConnect()`, `pollConnect()`, `cancelConnect()`, `kWifiConnectTimeoutMs`; keep `begin()` until Task 3
- Modify: `src/wifi_mgr.cpp` — implement the three new functions
- Modify: `include/ui.h` — add `showSplashProgress(int fill, bool wifiOk)`
- Modify: `src/ui.cpp` — implement `showSplashProgress()`
- Modify: `src/main.cpp` — two new state vars, refactored `setup()` splash block, background WiFi check in `loop()`, remove `WifiMgr::begin()` and dead WiFi helpers

---

### Task 1: Add non-blocking WiFi API to `wifi_mgr`

**Files:**
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`

- [ ] **Step 1: Update `wifi_mgr.h`**

Replace the entire content of `include/wifi_mgr.h` with:

```cpp
#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // WiFi connection timeout used by the splash loop in main.cpp
  constexpr unsigned long kWifiConnectTimeoutMs = 30000UL;

  // Load credentials and call WiFi.begin() — returns immediately (non-blocking).
  void startConnect();

  // Call each loop tick while connecting. Updates internal connected state on
  // first WL_CONNECTED. Returns true exactly once (the tick connection is made).
  bool pollConnect();

  // Abort connection attempt. Clears WiFi credentials from driver.
  void cancelConnect();

  bool isConnected();
  const char* activeSSID();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();
  bool runSerialSetup(void (*oledCb)(), bool (*cancelCb)());
  bool runEncoderEntry(const char* fieldName,
                       char* result, size_t maxLen,
                       int8_t (*deltaFn)(),
                       bool   (*shortFn)(),
                       bool   (*longFn)(),
                       void   (*oledFn)(const char* field, const char* buf, const char* sel));
}
```

- [ ] **Step 2: Add the three new functions to `wifi_mgr.cpp`**

After the closing `}` of `WifiMgr::begin()` (line 33), insert:

```cpp
void WifiMgr::startConnect() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String nvsSsid = prefs.getString("ssid", "");
  String nvsPass = prefs.getString("pass", "");
  prefs.end();

  if (nvsSsid.length() > 0) {
    nvsSsid.toCharArray(s_ssid, sizeof(s_ssid));
    nvsPass.toCharArray(s_pass, sizeof(s_pass));
  } else {
    strlcpy(s_ssid, WIFI_SSID_DEFAULT, sizeof(s_ssid));
    strlcpy(s_pass, WIFI_PASS_DEFAULT, sizeof(s_pass));
  }

  WiFi.setHostname("ANBRU-370");
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);
}

bool WifiMgr::pollConnect() {
  if (s_connected) return false;
  if (WiFi.status() == WL_CONNECTED) {
    s_connected = true;
    return true;
  }
  return false;
}

void WifiMgr::cancelConnect() {
  WiFi.disconnect(true);
}
```

- [ ] **Step 3: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: `SUCCESS` — `begin()` still exists so `main.cpp` compiles unchanged.

- [ ] **Step 4: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat: add startConnect/pollConnect/cancelConnect to WifiMgr"
```

---

### Task 2: Add `showSplashProgress()` to UI

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add declaration to `ui.h`**

After the `void showSplash();` line, insert:

```cpp
  // Redraws splash title with bottom-row WiFi status.
  // wifiOk=false: draws a 1px progress bar of `fill` pixels (0..128) at y=31.
  // wifiOk=true:  clears bottom strip and shows "WiFi Connected" centered at y=31.
  void showSplashProgress(int fill, bool wifiOk);
```

- [ ] **Step 2: Implement `showSplashProgress()` in `ui.cpp`**

After the closing `}` of `UI::showSplash()` (after line 30), insert:

```cpp
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
```

- [ ] **Step 3: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat: add showSplashProgress() — parallel WiFi status on splash"
```

---

### Task 3: Refactor `main.cpp` + remove `WifiMgr::begin()`

**Files:**
- Modify: `src/main.cpp`
- Modify: `include/wifi_mgr.h` (remove `begin()` declaration)
- Modify: `src/wifi_mgr.cpp` (remove `begin()` implementation)

- [ ] **Step 1: Add two new state variables to `main.cpp`**

After line 41 (`static unsigned long s_lastActivity = 0;`), insert:

```cpp
static bool s_wifiCancelled  = false;
static bool s_dcsBiosStarted = false;
```

- [ ] **Step 2: Replace the splash + WiFi block in `setup()`**

Find and replace this entire block (lines 144–174):

```cpp
  UI::setContrast(s_brightness);
  UI::showSplash();

  // Hold splash up to 30 s; any encoder input dismisses it early
  {
    unsigned long start = millis();
    while (millis() - start < 30000UL) {
      int8_t d = Encoder::readDelta();
      if (d != 0 || Encoder::shortPressed() || Encoder::longPressed()) break;
      delay(10);
    }
    Encoder::flush();
  }

  UI::showWifiConnecting(WIFI_SSID_DEFAULT);

  bool wifiOk = WifiMgr::begin();
  if (wifiOk) {
#ifndef RELEASE_BUILD
    Serial.printf("WiFi connected: %s\n", WifiMgr::activeSSID());
#endif
    UI::showWifiConnected(WifiMgr::activeSSID());
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   "255.255.255.255", DCSBIOS_CMD_PORT);
    UI::showSyncing();
  } else {
#ifndef RELEASE_BUILD
    Serial.printf("WiFi failed: %s\n", WifiMgr::activeSSID());
#endif
    UI::showWifiFailed(WifiMgr::activeSSID());
  }
```

With:

```cpp
  UI::setContrast(s_brightness);
  WifiMgr::startConnect();
  unsigned long wifiStart = millis();

  {
    while (millis() - wifiStart < WifiMgr::kWifiConnectTimeoutMs) {
      if (WifiMgr::pollConnect()) {
        unsigned long showStart = millis();
        while (millis() - showStart < 1500UL) {
          UI::showSplashProgress(128, true);
          if (Encoder::readDelta() || Encoder::shortPressed() || Encoder::longPressed()) break;
          delay(10);
        }
        DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                       "255.255.255.255", DCSBIOS_CMD_PORT);
        s_dcsBiosStarted = true;
        break;
      }
      unsigned long elapsed = millis() - wifiStart;
      int fill = (int)((elapsed * 128UL) / WifiMgr::kWifiConnectTimeoutMs);
      if (fill > 128) fill = 128;
      UI::showSplashProgress(fill, false);
      int8_t d  = Encoder::readDelta();
      bool   sp = Encoder::shortPressed();
      bool   lp = Encoder::longPressed();
      if (lp) { WifiMgr::cancelConnect(); s_wifiCancelled = true; break; }
      if (d != 0 || sp) break;
      delay(10);
    }
    Encoder::flush();
  }
```

- [ ] **Step 3: Add background WiFi check to `loop()`**

At the very top of `loop()`, before `bool dcsActivity = DcsBios::update();`, insert:

```cpp
  if (!s_dcsBiosStarted && !s_wifiCancelled) {
    if (WifiMgr::pollConnect()) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }
  }
```

- [ ] **Step 4: Remove `begin()` from `wifi_mgr.h`**

In `include/wifi_mgr.h`, remove the declaration `bool begin();` (now superseded by `startConnect()`).

- [ ] **Step 5: Remove `begin()` from `wifi_mgr.cpp`**

In `src/wifi_mgr.cpp`, delete the entire `WifiMgr::begin()` function:

```cpp
bool WifiMgr::begin() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String nvsSsid = prefs.getString("ssid", "");
  String nvsPass = prefs.getString("pass", "");
  prefs.end();

  if (nvsSsid.length() > 0) {
    nvsSsid.toCharArray(s_ssid, sizeof(s_ssid));
    nvsPass.toCharArray(s_pass, sizeof(s_pass));
  } else {
    strlcpy(s_ssid, WIFI_SSID_DEFAULT, sizeof(s_ssid));
    strlcpy(s_pass, WIFI_PASS_DEFAULT, sizeof(s_pass));
  }

  WiFi.setHostname("ANBRU-370");
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }
  s_connected = (WiFi.status() == WL_CONNECTED);
  return s_connected;
}
```

- [ ] **Step 6: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: `SUCCESS`, no errors, no new warnings.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat: parallel WiFi connect during splash — progress bar, long-press cancel"
```

---

## Hardware Verification

Flash and verify:

```bash
pio run -e esp32s3_supermini -t upload
```

**Normal boot (WiFi available):**
- Splash appears immediately with `AN/BRU-370` centered
- 1px bar grows left-to-right at the bottom (~4.3px/sec)
- When WiFi connects: bar replaced by "WiFi Connected" centered at bottom
- After ~1.5s (or encoder input): enters macro menu

**Long press during splash:**
- Splash exits immediately, macro menu appears
- Settings page shows `WiFi:--`
- DCS-BIOS never starts (no SYNCING/SYNCED message)

**Short press / encoder turn during splash:**
- Splash exits, macro menu appears immediately
- WiFi continues connecting in background
- Settings page shows `WiFi:--` then updates to `WiFi:OK` when connected
- SYNCING/SYNCED notification appears when DCS-BIOS receives first packet

**WiFi unavailable (30s timeout):**
- Bar reaches full width at ~30s, splash exits
- Settings page shows `WiFi:--`
- HID functions (gamepad, keyboard, mouse) work normally
