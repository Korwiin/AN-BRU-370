# OTA Firmware Update Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a manual HTTP OTA update flow reachable from Settings → Firmware that fetches a GitHub-hosted manifest, confirms the available version with the user, then downloads and flashes the binary.

**Architecture:** New `ota.cpp/h` module exposes two blocking functions — `check()` fetches the manifest and returns a result struct, `perform()` streams the binary into the ESP32 `Update` library. Main.cpp adds six `FIRMWARE_*` states to the menu enum and wires them to the OTA module and new UI screens. The Settings menu grows from 7 to 8 items.

**Tech Stack:** ESP32 Arduino core (`WiFiClientSecure`, `HTTPClient`, `Update`), U8g2 OLED, PlatformIO. No new `lib_deps` required.

> **Note:** The spec listed `perform(void(*progress)(int))`. This plan uses `perform(const char* url, void(*progress)(int))` — the URL must be passed from the `CheckResult` rather than stored globally.

---

## File Map

| Action | File | Responsibility |
|---|---|---|
| Modify | `include/config.h.example` | Add `OTA_MANIFEST_URL` placeholder |
| Modify | `include/config.h` | Add `OTA_MANIFEST_URL` with real value |
| Create | `ota/manifest.json` | Live manifest file served via GitHub raw |
| Create | `include/ota.h` | `CheckResult` struct + `check()` / `perform()` declarations |
| Create | `src/ota.cpp` | OTA module implementation |
| Modify | `include/ui.h` | Six new firmware UI function declarations |
| Modify | `src/ui.cpp` | Six firmware screens + Settings menu 7→8 items |
| Modify | `src/main.cpp` | `FIRMWARE_*` enum values, static vars, state handlers, OLED switch |

---

### Task 0: Config and manifest file

**Files:**
- Modify: `include/config.h.example`
- Modify: `include/config.h`
- Create: `ota/manifest.json`

- [ ] **Step 1: Add `OTA_MANIFEST_URL` to `config.h.example`**

Add this line after the existing `DEVICE_NAME` define:

```c
#define OTA_MANIFEST_URL "https://raw.githubusercontent.com/YOUR_GITHUB_USER/Brew370/main/ota/manifest.json"
```

- [ ] **Step 2: Add `OTA_MANIFEST_URL` to `config.h`**

Add the same line with your actual GitHub username replacing `YOUR_GITHUB_USER`:

```c
#define OTA_MANIFEST_URL "https://raw.githubusercontent.com/rsiemers/Brew370/main/ota/manifest.json"
```

(Substitute your real GitHub username.)

- [ ] **Step 3: Create `ota/manifest.json`**

Create the directory and file at the repo root (not inside `src/`). Initial content reflects the current shipped version so a device on v0.14 sees "Up to date":

```json
{"version": 20, "url": "https://github.com/YOUR_GITHUB_USER/Brew370/releases/download/v0.14/firmware.bin"}
```

`version` is the decimal value of `FIRMWARE_VERSION_BCD`. `0x0014` = 20.

- [ ] **Step 4: Verify build still passes**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS` — no code was changed.

- [ ] **Step 5: Commit**

```bash
git add include/config.h.example ota/manifest.json
git commit -m "feat: add OTA manifest URL config and initial manifest file"
```

(`config.h` is gitignored — do not add it.)

---

### Task 1: `ota.h` — module interface

**Files:**
- Create: `include/ota.h`

- [ ] **Step 1: Create `include/ota.h`**

```cpp
#pragma once
#include <Arduino.h>

namespace OTA {
  struct CheckResult {
    bool     available;
    uint16_t versionBCD;  // e.g. 0x0015; 0 on parse error
    char     url[256];    // release asset URL from manifest
    char     error[24];   // empty string on success; reason string on failure
  };

  // Fetch manifest from OTA_MANIFEST_URL and compare against FIRMWARE_VERSION_BCD.
  // Blocking (~1-3 s). Returns CheckResult with error[0] != 0 on any failure.
  CheckResult check();

  // Download binary from url and flash via ESP32 Update library.
  // Calls progress(0..100) periodically for OLED updates.
  // Blocking (tens of seconds). Calls ESP.restart() on success — never returns true.
  // Returns false on failure.
  bool perform(const char* url, void(*progress)(int));
}
```

- [ ] **Step 2: Verify build passes**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS` — header only, nothing linked yet.

- [ ] **Step 3: Commit**

```bash
git add include/ota.h
git commit -m "feat: add OTA module interface (ota.h)"
```

---

### Task 2: `OTA::check()` — manifest fetch and parse

**Files:**
- Create: `src/ota.cpp`

- [ ] **Step 1: Create `src/ota.cpp` with `check()` implementation**

```cpp
#include "ota.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

OTA::CheckResult OTA::check() {
  CheckResult result = {};

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, OTA_MANIFEST_URL)) {
    strlcpy(result.error, "No server", sizeof(result.error));
    return result;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    strlcpy(result.error, "No server", sizeof(result.error));
    return result;
  }

  String payload = http.getString();
  http.end();

  // Parse {"version": N, "url": "..."}
  const char* src = payload.c_str();

  const char* vp = strstr(src, "\"version\"");
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  vp = strchr(vp, ':');
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  result.versionBCD = (uint16_t)atoi(vp + 1);

  const char* up = strstr(src, "\"url\"");
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  up += 5;
  up = strchr(up, '"');
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  up++;
  const char* ue = strchr(up, '"');
  if (!ue) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  size_t len = (size_t)(ue - up);
  if (len >= sizeof(result.url)) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  memcpy(result.url, up, len);
  result.url[len] = '\0';

  result.available = (result.versionBCD > FIRMWARE_VERSION_BCD);
  return result;
}

bool OTA::perform(const char* url, void(*progress)(int)) {
  return false;  // placeholder — implemented in Task 3
}
```

- [ ] **Step 2: Verify build passes**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`. The `perform()` stub satisfies the linker.

- [ ] **Step 3: Commit**

```bash
git add src/ota.cpp
git commit -m "feat: implement OTA::check() — manifest fetch and parse"
```

---

### Task 3: `OTA::perform()` — download and flash

**Files:**
- Modify: `src/ota.cpp`

- [ ] **Step 1: Replace the `perform()` stub with the real implementation**

Replace the stub `perform()` function (last function in `src/ota.cpp`) with:

```cpp
bool OTA::perform(const char* url, void(*progress)(int)) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_RFC2616);
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  int totalBytes = http.getSize();
  if (!Update.begin(totalBytes > 0 ? totalBytes : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[512];
  int written = 0;

  while (http.connected() && (totalBytes < 0 || written < totalBytes)) {
    int avail = stream->available();
    if (avail > 0) {
      int toRead = min(avail, (int)sizeof(buf));
      int n = stream->readBytes(buf, toRead);
      Update.write(buf, n);
      written += n;
      if (totalBytes > 0 && progress) {
        progress((written * 100) / totalBytes);
      }
    }
    delay(1);
  }

  http.end();
  if (!Update.end(true)) return false;
  ESP.restart();
  return true;  // never reached
}
```

- [ ] **Step 2: Verify build passes**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add src/ota.cpp
git commit -m "feat: implement OTA::perform() — streaming download and flash"
```

---

### Task 4: UI screens for firmware states

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add six declarations to `include/ui.h`**

Add after the `showWifiSubMenu` declaration (before `showCharEntry`):

```cpp
  // OTA firmware update screens
  void showFirmwareMenu(const char* currentVer);
  void showFirmwareChecking();
  void showFirmwareUpToDate(const char* ver);
  void showFirmwareConfirm(const char* availVer);
  void showFirmwareUpdating(int percent);
  void showFirmwareError(const char* reason);
```

- [ ] **Step 2: Implement the six screens in `src/ui.cpp`**

Add at the end of `src/ui.cpp`, before the closing of the file:

```cpp
// ---- Firmware update screens ----

void UI::showFirmwareMenu(const char* currentVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8,  "AN/BRU-370");
  u8g2.drawStr(0, 16, currentVer);
  u8g2.drawStr(0, 24, "Firmware");
  u8g2.drawStr(65, 8,  "> Check");
  u8g2.drawStr(65, 16, "LP=Back");
  u8g2.sendBuffer();
}

void UI::showFirmwareChecking() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* msg = "Checking...";
  u8g2.drawStr((128 - u8g2.getStrWidth(msg)) / 2, 20, msg);
  u8g2.sendBuffer();
}

void UI::showFirmwareUpToDate(const char* ver) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* line1 = "Up to date";
  u8g2.drawStr((128 - u8g2.getStrWidth(line1)) / 2, 14, line1);
  u8g2.drawStr((128 - u8g2.getStrWidth(ver))   / 2, 24, ver);
  u8g2.sendBuffer();
}

void UI::showFirmwareConfirm(const char* availVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  char line1[20];
  snprintf(line1, sizeof(line1), "%s avail", availVer);
  u8g2.drawStr(0, 8,  line1);
  u8g2.drawStr(0, 18, "SP=Update");
  u8g2.drawStr(0, 28, "LP=Cancel");
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

void UI::showFirmwareError(const char* reason) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "Update failed");
  u8g2.drawStr(0, 21, reason);
  u8g2.drawStr(0, 31, "SP=Back");
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Verify build passes**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat: add OTA firmware UI screens (showFirmwareMenu et al.)"
```

---

### Task 5: Expand Settings menu 7 → 8 items

**Files:**
- Modify: `src/ui.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Update `s_menuItems` and `kNumMenuItems` in `src/ui.cpp`**

Current (`src/ui.cpp:182`):
```cpp
static const char* s_menuItems[] = {
  "Knob","Brightness","LCD Sleep","WiFi","Mouse Tune","Reboot","EXIT"
};
static const int kNumMenuItems = 7;
```

Replace with:
```cpp
static const char* s_menuItems[] = {
  "Knob","Brightness","LCD Sleep","WiFi","Mouse Tune","Firmware","Reboot","EXIT"
};
static const int kNumMenuItems = 8;
```

- [ ] **Step 2: Add `"Fw Upd"` label alias in `showSettingsMenu`**

Current label block in `showSettingsMenu` (`src/ui.cpp` ~line 205):
```cpp
    if (idx == 0)      label = encReversed ? "Knob:CCW" : "Knob:CW";
    else if (idx == 1) label = "Bright";
    else if (idx == 4) label = "Mouse";
    else               label = s_menuItems[idx];
```

Replace with:
```cpp
    if (idx == 0)      label = encReversed ? "Knob:CCW" : "Knob:CW";
    else if (idx == 1) label = "Bright";
    else if (idx == 4) label = "Mouse";
    else if (idx == 5) label = "Fw Upd";
    else               label = s_menuItems[idx];
```

- [ ] **Step 3: Update the SETTINGS rotation modulo in `src/main.cpp`**

Current (`src/main.cpp:387`):
```cpp
    s_menuSel = (s_menuSel + delta + 7) % 7;
```

Replace with:
```cpp
    s_menuSel = (s_menuSel + delta + 8) % 8;
```

- [ ] **Step 4: Update `executeMenuItem()` in `src/main.cpp`**

Current `case 5` and `case 6` (`src/main.cpp:108`):
```cpp
    case 5:  // Reboot
      ESP.restart();
      break;
    case 6:  // EXIT
      s_mode = MACRO_MENU;
      return;
```

Replace with:
```cpp
    case 5:  // Firmware
      s_mode = FIRMWARE_MENU;
      return;
    case 6:  // Reboot
      ESP.restart();
      break;
    case 7:  // EXIT
      s_mode = MACRO_MENU;
      return;
```

- [ ] **Step 5: Verify build passes**

```bash
pio run -e esp32s3_supermini
```

Expected: compiler error — `FIRMWARE_MENU` is not declared yet. This is expected; Task 6 adds it. If you want a clean build first, add a temporary `FIRMWARE_MENU` dummy to the enum in `main.cpp` and remove it in Task 6.

Actually: proceed to Task 6 immediately without a separate commit here — the build won't pass until the enum is extended.

---

### Task 6: Main loop state machine

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add `#include "ota.h"` to `src/main.cpp`**

Add after the existing includes at the top of `src/main.cpp`:
```cpp
#include "ota.h"
```

- [ ] **Step 2: Add `FIRMWARE_*` values to the `MenuState` enum (`src/main.cpp:12`)**

Current:
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT
};
```

Replace with:
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT,
  FIRMWARE_MENU, FIRMWARE_CHECKING, FIRMWARE_UP_TO_DATE,
  FIRMWARE_CONFIRM, FIRMWARE_UPDATING, FIRMWARE_ERROR
};
```

- [ ] **Step 3: Add OTA static variables after the existing statics (~line 50)**

Add after `static bool s_wifiEnabled = true;`:
```cpp
static OTA::CheckResult  s_otaResult        = {};
static unsigned long     s_otaUpToDateSince  = 0;
static char              s_otaError[24]      = {0};
```

- [ ] **Step 4: Add `otaProgressCb` before `executeMenuItem()` (~line 87)**

Add immediately before `static void executeMenuItem()`:
```cpp
static void otaProgressCb(int percent) {
  UI::showFirmwareUpdating(percent);
}
```

- [ ] **Step 5: Add FIRMWARE_* state handlers in the main input-handling block**

The WiFi state handlers end around line 582 with:
```cpp
      s_wifiSubSel = 0; s_wifiSubOffset = 0; s_mode = SETTINGS;
    }
  }
```

Add the following block immediately after that closing brace (before `// OLED sleep check`):

```cpp
  } else if (s_mode == FIRMWARE_MENU) {
    if (Encoder::shortPressed()) s_mode = FIRMWARE_CHECKING;
    if (Encoder::longPressed())  { s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0; }

  } else if (s_mode == FIRMWARE_CHECKING) {
    if (!WifiMgr::isConnected()) {
      strlcpy(s_otaError, "No WiFi", sizeof(s_otaError));
      s_mode = FIRMWARE_ERROR;
    } else {
      UI::showFirmwareChecking();
      s_otaResult = OTA::check();
      if (s_otaResult.error[0]) {
        strlcpy(s_otaError, s_otaResult.error, sizeof(s_otaError));
        s_mode = FIRMWARE_ERROR;
      } else if (s_otaResult.available) {
        s_mode = FIRMWARE_CONFIRM;
      } else {
        s_otaUpToDateSince = millis();
        s_mode = FIRMWARE_UP_TO_DATE;
      }
    }

  } else if (s_mode == FIRMWARE_UP_TO_DATE) {
    if (Encoder::shortPressed() || millis() - s_otaUpToDateSince > 3000) {
      s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0;
    }

  } else if (s_mode == FIRMWARE_CONFIRM) {
    if (Encoder::shortPressed()) s_mode = FIRMWARE_UPDATING;
    if (Encoder::longPressed())  { s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0; }

  } else if (s_mode == FIRMWARE_UPDATING) {
    UI::showFirmwareUpdating(0);
    if (!OTA::perform(s_otaResult.url, otaProgressCb)) {
      strlcpy(s_otaError, "Flash failed", sizeof(s_otaError));
      s_mode = FIRMWARE_ERROR;
    }

  } else if (s_mode == FIRMWARE_ERROR) {
    if (Encoder::shortPressed()) { s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0; }
```

- [ ] **Step 6: Add FIRMWARE_* cases to the OLED switch (~line 594)**

In the `switch (s_mode)` block inside the OLED update section, add after `case WIFI_MENU:`:

```cpp
      case FIRMWARE_MENU: {
        char ver[12];
        snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
        UI::showFirmwareMenu(ver);
        break;
      }
      case FIRMWARE_CHECKING:  break;  // screen set inline before blocking call
      case FIRMWARE_UP_TO_DATE: {
        char ver[12];
        snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
        UI::showFirmwareUpToDate(ver);
        break;
      }
      case FIRMWARE_CONFIRM: {
        char ver[10];
        snprintf(ver, sizeof(ver), "%X.%02X",
                 s_otaResult.versionBCD >> 8, s_otaResult.versionBCD & 0xFF);
        UI::showFirmwareConfirm(ver);
        break;
      }
      case FIRMWARE_UPDATING:  break;  // screen driven by otaProgressCb
      case FIRMWARE_ERROR:     UI::showFirmwareError(s_otaError); break;
```

- [ ] **Step 7: Verify clean build**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS` with no warnings about unhandled enum cases.

- [ ] **Step 8: Commit Tasks 5 and 6 together**

```bash
git add src/main.cpp src/ui.cpp include/ui.h
git commit -m "feat: wire OTA state machine into Settings menu (v0.15)"
```

---

### Task 7: Version bump and flash

**Files:**
- Modify: `include/config.h`

- [ ] **Step 1: Bump version in `config.h`**

```c
#define FIRMWARE_VERSION     "0.15"
#define FIRMWARE_VERSION_BCD  0x0015
```

- [ ] **Step 2: Build release binary**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`. Note the `.pio/build/esp32s3_supermini/firmware.bin` size — should be under 1,280 KB.

- [ ] **Step 3: Flash to device**

```bash
pio run -e esp32s3_supermini -t upload
```

- [ ] **Step 4: Verify Settings menu**

Navigate: long-press encoder (or however Settings is entered) → scroll to **Fw Upd** (6th item) → short-press.

Expected: Firmware menu shows `v0.15` on left, `> Check / LP=Back` on right.

- [ ] **Step 5: Verify "No WiFi" error path**

With WiFi disabled (Settings → WiFi → WiFi:OFF, reboot) — navigate to Firmware → Check.

Expected: `FIRMWARE_ERROR` screen shows `Update failed / No WiFi`. Short-press returns to Settings.

- [ ] **Step 6: Verify "Up to date" path**

With WiFi connected and manifest `version` = 21 set locally to 20 (matching `FIRMWARE_VERSION_BCD` 0x0015 = 21).

Wait — `0x0015` = 21 decimal. The current manifest has `"version": 20`. So v0.15 (`FIRMWARE_VERSION_BCD` = 0x0015 = 21) will see manifest version 20, which is < 21, so "Up to date" is shown.

Navigate: Firmware → Check (WiFi connected).

Expected: `FIRMWARE_UP_TO_DATE` screen shows `Up to date / v0.15`. Auto-returns to Settings after 3 s or short-press.

- [ ] **Step 7: Verify "confirm" path by temporarily bumping manifest version**

Temporarily edit `ota/manifest.json` to `"version": 22` (do not commit yet). Push to GitHub main. Navigate: Firmware → Check.

Expected: `FIRMWARE_CONFIRM` screen shows `0.16 avail / SP=Update / LP=Cancel`.

Short-press `LP` (long-press): returns to Settings.

Revert `ota/manifest.json` back to `"version": 21` and push.

- [ ] **Step 8: Commit version bump**

```bash
git add include/config.h.example
git commit -m "release: bump to v0.15 — OTA firmware update"
```

(`config.h` is gitignored — do not add.)

---

## Self-Review

**Spec coverage:**
- ✅ Section 1 (module interface): Tasks 1–3
- ✅ Section 2 (manifest + GitHub URLs): Task 0
- ✅ Section 3 (UX flow / OLED states): Tasks 4 + 6 — all six states covered
- ✅ Section 4 (Settings menu 7→8): Task 5
- ✅ Section 5 (config + build): Task 0
- ✅ Section 6 (release workflow): Task 7 documents the bump process
- ✅ Section 7 (flash budget): no action required — partition table unchanged

**Interface note:** `perform()` takes `const char* url` as first arg (not in spec). This is intentional and necessary — the URL comes from `CheckResult.url`.

**Type consistency check:**
- `CheckResult.error` set by `check()`, read in Task 6 state handler ✅
- `CheckResult.url` passed to `perform()` in Task 6 ✅
- `CheckResult.versionBCD` formatted with `%X.%02X` in OLED switch ✅
- `UI::showFirmwareUpdating(int)` called from `otaProgressCb(int)` ✅
- `kNumMenuItems` matches `s_menuItems[]` array length (both 8) ✅
- `executeMenuItem()` case 7 matches EXIT (was 6) ✅
