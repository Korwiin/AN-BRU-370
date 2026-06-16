# BLE UX Hardening + Credential Security Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove compiled-in WiFi credentials, handle empty-NVS first boot, and fix BLE setup UX (greeting timing, ANSI banner, OLED state, empty password support).

**Architecture:** Three independent slices — credential security (wifi_mgr + config), empty-NVS boot gate (main + ui), BLE state machine hardening (wifi_mgr + ui). Each slice compiles and is verifiable on its own; implement in order since Task 3 depends on Task 1's `hasCredentials()`.

**Tech Stack:** ESP32-S3 / PlatformIO / Arduino; NimBLE (NUS UART); Preferences (NVS); u8g2 OLED; ANSI escape codes

**Spec:** `docs/superpowers/specs/2026-06-16-ble-ux-hardening.md`

---

## File Map

| File | Change |
|------|--------|
| `include/config.h.example` | Remove `WIFI_SSID_DEFAULT` / `WIFI_PASS_DEFAULT` defines |
| `include/wifi_mgr.h` | Add `hasCredentials()`, `isBleClientConnected()` declarations |
| `include/ui.h` | Change `showBleActive()` → `showBleActive(bool)`; add `showNoCredentials()` |
| `src/wifi_mgr.cpp` | `loadCredentials()` no fallback; `beginAttempt()` empty-cred guard; add `hasCredentials()`, `isBleClientConnected()`; `BleTxSubscribeCb`; `s_bleSubscribed`; `sendBanner()`; ANSI prompts; empty-pass accept |
| `src/ui.cpp` | `showBleActive(bool)` WAITING/CONNECTED; add `showNoCredentials()` |
| `src/main.cpp` | Empty-NVS check in `setup()`; update BLE callback to pass `isBleClientConnected()` |

---

## Task 1: Credential Security — Remove Compiled-In SSID/Password

**Files:**
- Modify: `include/config.h.example`
- Modify: `src/wifi_mgr.cpp` — `loadCredentials()` and `beginAttempt()`
- Modify: `include/wifi_mgr.h`

### Context

`loadCredentials()` (wifi_mgr.cpp:33-46) currently falls back to `WIFI_SSID_DEFAULT` / `WIFI_PASS_DEFAULT` from `config.h` when NVS is empty. Those defines compile into every binary including OTA releases on GitHub. The fix: NVS-only; if empty, set empty strings and let `beginAttempt()` catch it.

- [ ] **Step 1: Remove the fallback defines from config.h.example**

Open `include/config.h.example`. Remove these two lines entirely:
```cpp
// DELETE these two lines:
#define WIFI_SSID_DEFAULT   "YourNetworkName"
#define WIFI_PASS_DEFAULT   "YourPassword"
```

- [ ] **Step 2: Fix `loadCredentials()` — remove the else fallback**

In `src/wifi_mgr.cpp`, replace the current `loadCredentials()` (lines 33-46):

```cpp
static void loadCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String nvsSsid = prefs.getString("ssid", "");
  String nvsPass = prefs.getString("pass", "");
  prefs.end();
  nvsSsid.toCharArray(s_ssid, sizeof(s_ssid));
  nvsPass.toCharArray(s_pass, sizeof(s_pass));
}
```

- [ ] **Step 3: Add empty-credential guard in `beginAttempt()`**

In `src/wifi_mgr.cpp`, immediately after the `loadCredentials()` call inside `beginAttempt()` (around line 74), add:

```cpp
if (s_ssid[0] == '\0') {
  s_phase_ssidFail = true;
  return false;
}
```

- [ ] **Step 4: Add `hasCredentials()` declaration to header**

In `include/wifi_mgr.h`, add alongside the other `WifiMgr` declarations:

```cpp
bool hasCredentials();
```

- [ ] **Step 5: Add `hasCredentials()` implementation**

In `src/wifi_mgr.cpp`, add after `loadCredentials()`:

```cpp
bool WifiMgr::hasCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid.length() > 0;
}
```

- [ ] **Step 6: Compile and verify**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run
```

Expected: zero errors, zero warnings about `WIFI_SSID_DEFAULT` or `WIFI_PASS_DEFAULT`.

- [ ] **Step 7: Commit**

```bash
git add include/config.h.example include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "security: remove compiled-in WiFi credentials; NVS-only from now on"
```

---

## Task 2: Empty NVS Boot Flow — "No WiFi Setup" Screen

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`
- Modify: `src/main.cpp`

### Context

When `hasCredentials()` is false, the boot sequence should show a "No WiFi Setup / Press to continue" screen, wait for an encoder press, then navigate to `WIFI_MENU`. This replaces the normal boot status / WiFi connect sequence.

Look at existing `UI::showBleActive()` in `src/ui.cpp` for OLED centering pattern. Look at `src/main.cpp` setup() USB settle loop (~3 seconds) for where to insert the check. `WIFI_MENU` is an existing `MenuState` enum value in `main.cpp`.

- [ ] **Step 1: Add `showNoCredentials()` declaration**

In `include/ui.h`, add alongside other `UI` function declarations:

```cpp
void showNoCredentials();
```

- [ ] **Step 2: Implement `showNoCredentials()`**

In `src/ui.cpp`, add before or after `showBleActive()`:

```cpp
void UI::showNoCredentials() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  int w1 = u8g2.getStrWidth("No WiFi Setup");
  int w2 = u8g2.getStrWidth("Press to cont.");
  u8g2.drawStr((128 - w1) / 2,  8, "No WiFi Setup");
  u8g2.drawStr((128 - w2) / 2, 32, "Press to cont.");
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Add empty-NVS check in `setup()`**

In `src/main.cpp`, after the 3-second USB settle loop (the `while (millis() < 3000)` block) and before the `beginAttempt(1)` call or the `BOOT_STATUS` state initialization, add:

```cpp
if (!WifiMgr::hasCredentials()) {
  UI::showNoCredentials();
  while (!encoder.shortPressed() && !encoder.longPressed()) {
    encoder.readDelta();
    delay(10);
  }
  s_mode = WIFI_MENU;
  return;  // skip WiFi boot sequence entirely
}
```

Ensure `#include "wifi_mgr.h"` is already present at the top of `main.cpp` (it should be).

- [ ] **Step 4: Compile**

```bash
pio run
```

Expected: clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/ui.h src/ui.cpp src/main.cpp
git commit -m "feat: show No WiFi Setup screen and route to WiFi menu when NVS empty"
```

---

## Task 3: BLE — onSubscribe Trigger + ANSI Banner

**Files:**
- Modify: `src/wifi_mgr.cpp`

### Context

**Current bug:** `WAIT_CLIENT` fires the greeting when `onConnect` sets `s_bleClientConn = true`. At that moment, the client has not yet subscribed to the TX NOTIFY characteristic, so the greeting is lost. The fix is a second volatile flag `s_bleSubscribed`, set only from an `onSubscribe` callback on the TX characteristic.

The current `runBleSetup()` starts at line 388. Key references:
- `BleServerCb::onConnect/onDisconnect` — lines ~344-354
- `s_bleTxChar` — line 12
- `WAIT_CLIENT` state — lines 419-427
- `bleSend()` — lines 373-386

The ANSI banner replaces the current plain-text greeting. Use a `sendBanner()` static helper.

- [ ] **Step 1: Add `s_bleSubscribed` volatile flag**

Near the other static BLE state variables (around line 12-16 in `wifi_mgr.cpp`), add:

```cpp
static volatile bool s_bleSubscribed = false;
```

- [ ] **Step 2: Add `BleTxSubscribeCb` class**

Add after `BleRxCb` class (around line 370):

```cpp
class BleTxSubscribeCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
    if (subValue & 0x0001) {
      s_bleSubscribed = true;
    }
  }
};
```

- [ ] **Step 3: Clear `s_bleSubscribed` on disconnect**

In `BleServerCb::onDisconnect()` (around line 348), add:

```cpp
s_bleSubscribed = false;
```

So the full callback reads:

```cpp
void onDisconnect(NimBLEServer* pServer) override {
  s_bleSubscribed  = false;
  s_bleClientConn  = false;
  s_bleLineReady   = false;
  s_bleRxBuf       = "";
  pServer->startAdvertising();
}
```

- [ ] **Step 4: Attach subscribe callback to TX characteristic**

In `runBleSetup()`, after `s_bleTxChar` is created (around line 398), add:

```cpp
s_bleTxChar->setCallbacks(new BleTxSubscribeCb());
```

- [ ] **Step 5: Add `sendBanner()` static helper**

Add before `runBleSetup()` (around line 388):

```cpp
static void sendBanner() {
  char msg[512];
  snprintf(msg, sizeof(msg),
    "\033[2J\033[H"
    "\033[0;31m[UNCLASSIFIED]\033[0m\r\n"
    "\r\n"
    "\033[1;32mAN/BRU-370 CONFIG TERMINAL\033[0m\r\n"
    "\033[1;32mF-16C Bl.50 // USAF\033[0m\r\n"
    "\r\n"
    "\033[0;31mWARNING: RESTRICTED SYSTEM\033[0m\r\n"
    "\033[0;31mUnauthorized use prohibited.\033[0m\r\n"
    "\r\n"
    "\033[0;32mCurrent SSID: %s\033[0m\r\n"
    "\r\n"
    "Enter \033[1;32mSSID\033[0;32m:\033[0m \r\n",
    s_ssid[0] ? s_ssid : "(none)");
  bleSend(msg);
}
```

- [ ] **Step 6: Update `WAIT_CLIENT` case to use `s_bleSubscribed` and `sendBanner()`**

Replace the current `WAIT_CLIENT` case (lines 419-427):

```cpp
case WAIT_CLIENT:
  if (s_bleSubscribed) {
    sendBanner();
    state = GET_SSID;
  }
  break;
```

- [ ] **Step 7: Compile**

```bash
pio run
```

Expected: clean compile.

- [ ] **Step 8: Commit**

```bash
git add src/wifi_mgr.cpp
git commit -m "feat(ble): send greeting on subscribe not connect; ANSI banner"
```

---

## Task 4: BLE — ANSI Prompts, Empty Password, Confirmation Format

**Files:**
- Modify: `src/wifi_mgr.cpp`

### Context

Three small changes to the BLE state machine's GET_SSID, GET_PASS, and CONFIRM cases (lines 430-491):

1. Bold ANSI field name labels on all prompts
2. Accept empty password (open network) without re-prompting
3. Confirmation shows `(none)` for empty password; confirmation prompt uses ANSI bold

- [ ] **Step 1: Update SSID rejection prompt in `GET_SSID`**

In `GET_SSID` case, replace:
```cpp
bleSend("Enter new SSID:\r\n");
```
With:
```cpp
bleSend("Enter \033[1;32mSSID\033[0;32m:\033[0m \r\n");
```

And replace the first call to enter SSID (in `sendBanner()` — already done in Task 3 step 5).

- [ ] **Step 2: Replace `GET_PASS` to accept empty input and use ANSI prompt**

Replace the current `GET_PASS` case (lines 448-464) with:

```cpp
case GET_PASS:
  if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
  if (s_bleLineReady) {
    portENTER_CRITICAL(&s_bleMux);
    String line = s_bleRxBuf;
    s_bleRxBuf = ""; s_bleLineReady = false;
    portEXIT_CRITICAL(&s_bleMux);
    line.trim();
    strlcpy(newPass, line.c_str(), sizeof(newPass));
    const char* passDisplay = (newPass[0] == '\0') ? "(none)" : newPass;
    char msg[256];
    snprintf(msg, sizeof(msg),
      "\033[0;32m\r\nCurrent SSID: %s\r\n\r\n"
      "  \033[1;32mSSID\033[0;32m: %s\r\n"
      "  \033[1;32mPass\033[0;32m: %s\033[0m\r\n"
      "\r\n"
      "Save [\033[1;32mY\033[0;32m / \033[1;32mr\033[0;32m=retry / \033[1;32mn\033[0;32m=cancel\033[0m]:\r\n",
      s_ssid[0] ? s_ssid : "(none)", newSSID, passDisplay);
    bleSend(msg);
    state = CONFIRM;
  }
  break;
```

- [ ] **Step 3: Update `GET_PASS` entry prompt**

In `GET_SSID` case, when accepting valid SSID, replace:
```cpp
bleSend("Enter Password:\r\n");
```
With:
```cpp
bleSend("Enter \033[1;32mPassword\033[0;32m:\033[0m \r\n");
```

- [ ] **Step 4: Update retry prompt in `CONFIRM` case**

In the `CONFIRM` case `r` branch (around line 480-485), replace the snprintf/bleSend block:

```cpp
} else if (line == "r" || line == "R") {
  bleSend("\033[0;32mEnter \033[1;32mSSID\033[0;32m:\033[0m \r\n");
  state = GET_SSID;
}
```

- [ ] **Step 5: Compile**

```bash
pio run
```

Expected: clean compile.

- [ ] **Step 6: Commit**

```bash
git add src/wifi_mgr.cpp
git commit -m "feat(ble): ANSI prompts, accept empty password, formatted confirmation"
```

---

## Task 5: BLE OLED — WAITING / CONNECTED States

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`
- Modify: `src/main.cpp`

### Context

`showBleActive()` currently takes no arguments and always shows static text. Change it to accept a `bool connected` parameter and show `--== WAITING ==--` vs `--== CONNECTED ==--`. Also need `WifiMgr::isBleClientConnected()` so `main.cpp` can pass the current state.

Layout (128×32 OLED, `u8g2_font_5x7_tr`):
- y=8:  `AN/BRU-370 BLUETOOTH` centered
- y=16: `--== WAITING ==--` or `--== CONNECTED ==--` centered
- y=24: blank
- y=32: `LP to Cancel` centered

- [ ] **Step 1: Update `showBleActive` declaration in ui.h**

In `include/ui.h`, change:
```cpp
void showBleActive();
```
To:
```cpp
void showBleActive(bool connected);
```

- [ ] **Step 2: Update `showBleActive` implementation in ui.cpp**

Replace the existing `showBleActive()` body in `src/ui.cpp`:

```cpp
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
```

- [ ] **Step 3: Add `isBleClientConnected()` declaration to wifi_mgr.h**

In `include/wifi_mgr.h`, add:

```cpp
bool isBleClientConnected();
```

- [ ] **Step 4: Add `isBleClientConnected()` implementation**

In `src/wifi_mgr.cpp`, add near `hasCredentials()`:

```cpp
bool WifiMgr::isBleClientConnected() { return s_bleClientConn; }
```

- [ ] **Step 5: Update BLE callback in main.cpp**

In `src/main.cpp`, find the lambda passed to `runBleSetup()` as `oledActiveCb`. Update it to:

```cpp
[]() { UI::showBleActive(WifiMgr::isBleClientConnected()); }
```

- [ ] **Step 6: Compile**

```bash
pio run
```

Expected: clean compile, no calls to the old zero-argument `showBleActive()`.

- [ ] **Step 7: Bump version**

In `include/config.h.example`, update:
```cpp
#define FIRMWARE_VERSION     "0.27"
#define FIRMWARE_VERSION_BCD  0x0027
```

Update `include/config.h` with the same version bump.

- [ ] **Step 8: Commit**

```bash
git add include/ui.h src/ui.cpp include/wifi_mgr.h src/wifi_mgr.cpp src/main.cpp include/config.h.example
git commit -m "feat(ble): OLED shows WAITING/CONNECTED states; bump v0.27"
```

---

## Manual Testing Checklist (after flash)

- [ ] **Empty NVS first boot**: power on with no credentials → "No WiFi Setup / Press to cont." → encoder press → WiFi menu appears
- [ ] **BLE happy path**: Bluefruit → connect → open UART tab → ANSI banner appears (not before UART tab) → enter SSID → enter password → confirm → Y → "Credentials saved. Rebooting..." → device reboots → connects
- [ ] **BLE OLED states**: while waiting for Bluefruit to open UART → `--== WAITING ==--`; after opening UART → `--== CONNECTED ==--`
- [ ] **Disconnect mid-SSID**: open UART, wait for banner, then close Bluefruit app mid-entry → device returns to WAITING; reconnect + open UART → banner fires again from top
- [ ] **Empty password**: enter SSID → press Enter on blank password → confirm shows `Pass: (none)` → Y → saves → reboots → connects to open network (or fails cleanly with "SSID not found" if network not reachable — not a firmware bug)
- [ ] **Retry**: at confirmation prompt, send `r` → back to SSID prompt (no banner refire); enter new SSID + password → Y → saves
- [ ] **Cancel**: at confirmation prompt, send `n` → "Cancelled." → BLE exits; device returns to WiFi menu
- [ ] **`config.h.example` audit**: `grep WIFI_SSID_DEFAULT include/config.h.example` → no output
