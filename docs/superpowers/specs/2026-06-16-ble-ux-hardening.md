# BLE UX Hardening + Credential Security

**Version target:** v0.27
**Status:** Ready for implementation

---

## Overview

Three related changes:

1. **BLE UX hardening** — fix greeting timing, ANSI banner, OLED state, disconnect restart
2. **Credential security** — remove compiled-in SSID/password; NVS-only from now on
3. **Empty NVS boot flow** — guide user to WiFi menu on first boot with no credentials

---

## 1. Credential Security

### Problem

`loadCredentials()` falls back to `WIFI_SSID_DEFAULT` / `WIFI_PASS_DEFAULT` from `config.h` if NVS is empty. Those defines are compiled into every binary including OTA releases on GitHub.

### Changes

**`include/config.h.example`** — remove the two defines entirely:

```cpp
// Remove these two lines:
// #define WIFI_SSID_DEFAULT   "YourNetworkName"
// #define WIFI_PASS_DEFAULT   "YourPassword"
```

**`src/wifi_mgr.cpp` — `loadCredentials()`** — remove fallback:

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

**`include/wifi_mgr.h`** — add declaration:

```cpp
bool hasCredentials();
```

**`src/wifi_mgr.cpp`** — add implementation:

```cpp
bool WifiMgr::hasCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid.length() > 0;
}
```

**`src/wifi_mgr.cpp` — `beginAttempt()`** — guard against empty credentials after `loadCredentials()`:

```cpp
if (s_ssid[0] == '\0') {
  s_phase_ssidFail = true;
  return false;
}
```

---

## 2. Empty NVS Boot Flow

When `hasCredentials()` returns false at startup, skip the WiFi boot sequence and guide the user to configure credentials first.

### OLED screen

```
  No WiFi Setup   ← centered, line 1 (y=8)
                  ← blank, line 2
                  ← blank, line 3
  Press to cont.  ← centered, line 4 (y=32)
```

### `include/ui.h` — add declaration:

```cpp
void showNoCredentials();
```

### `src/ui.cpp` — add implementation:

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

### `src/main.cpp` — in `setup()`, after the 3-second USB settle loop:

```cpp
if (!WifiMgr::hasCredentials()) {
  UI::showNoCredentials();
  // Wait for any encoder press
  while (!encoder.shortPressed() && !encoder.longPressed()) {
    encoder.readDelta();
    delay(10);
  }
  s_mode = WIFI_MENU;
  return;  // skip WiFi boot sequence
}
```

---

## 3. BLE UX Hardening

### 3a. Send greeting on subscribe, not on connect

**Problem:** `WAIT_CLIENT` fires the greeting when `onConnect` sets `s_bleClientConn = true`. The client has not yet subscribed to the TX characteristic's NOTIFY property, so the first message is lost.

**Fix:** Add a second volatile flag `s_bleSubscribed`. Set it in an `onSubscribe` callback on the TX characteristic. Gate the greeting on this flag instead of `s_bleClientConn`.

```cpp
static volatile bool s_bleSubscribed = false;

class BleTxSubscribeCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
    if (subValue & 0x0001) {   // notify bit
      s_bleSubscribed = true;
    }
  }
};
```

In `runBleSetup()`, attach to the TX characteristic:

```cpp
s_bleTxChar->setCallbacks(new BleTxSubscribeCb());
```

In `BleServerCb::onDisconnect()`, also clear the subscribe flag:

```cpp
s_bleSubscribed = false;
```

In the state machine, replace `s_bleClientConn` with `s_bleSubscribed` for the greeting trigger:

```cpp
case WAIT_CLIENT:
  if (s_bleSubscribed) {
    sendBanner();   // see §3b
    state = GET_SSID;
  }
  break;
```

Mid-session disconnect handling (already in code, keep as-is):

```cpp
case GET_SSID:
  if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
  // ...
```

When a client disconnects mid-session, `s_bleSubscribed` clears via `onDisconnect`. On the next subscribe from the same or different client, the banner fires again from the top.

### 3b. ANSI greeting banner

Extract to a `static void sendBanner()` helper:

```cpp
static void sendBanner() {
  char msg[512];
  snprintf(msg, sizeof(msg),
    "\033[2J\033[H"                          // clear screen, home cursor
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

Note: `bleSend()` sends in 20-byte NUS chunks. The banner is ~350 bytes — `bleSend()` already handles chunking so no change needed there.

### 3c. Bolded prompts

Replace bare prompt strings with ANSI-bolded field names:

| Old | New |
|-----|-----|
| `"Enter new SSID:\r\n"` | `"Enter \033[1;32mSSID\033[0;32m:\033[0m \r\n"` |
| `"Enter Password:\r\n"` | `"Enter \033[1;32mPassword\033[0;32m:\033[0m \r\n"` |
| `"Save & Reboot (Y), Retry (r), Cancel (n):\r\n"` | `"Save [\033[1;32mY\033[0;32m / \033[1;32mr\033[0;32m=retry / \033[1;32mn\033[0;32m=cancel\033[0m]:\r\n"` |

### 3d. Confirmation format with empty password

In `GET_PASS` → `CONFIRM` transition:

```cpp
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
```

### 3e. Allow empty password (open networks)

In `GET_PASS`, remove SSID-style empty check — accept blank input directly:

```cpp
case GET_PASS:
  if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
  if (s_bleLineReady) {
    // accept line as-is — empty = open network
    portENTER_CRITICAL(&s_bleMux);
    String line = s_bleRxBuf;
    s_bleRxBuf = ""; s_bleLineReady = false;
    portEXIT_CRITICAL(&s_bleMux);
    line.trim();
    strlcpy(newPass, line.c_str(), sizeof(newPass));
    // ... build confirm message
  }
  break;
```

### 3f. After Y — close BLE then reboot

Current code already does `result = true; done = true;` → caller in `main.cpp` should call `ESP.restart()`. Verify the call site in `main.cpp` handles this:

```cpp
if (WifiMgr::runBleSetup(...)) {
  ESP.restart();
}
```

Message sent before reboot: `"Credentials saved. Rebooting...\r\n"` (already in code).

### 3g. OLED — WAITING / CONNECTED states

**`include/ui.h`** — change signature:

```cpp
void showBleActive(bool connected);
```

**`src/ui.cpp`** — update implementation:

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

**`include/wifi_mgr.h`** — add getter:

```cpp
bool isBleClientConnected();
```

**`src/wifi_mgr.cpp`** — add implementation:

```cpp
bool WifiMgr::isBleClientConnected() { return s_bleClientConn; }
```

**`src/main.cpp`** — update BLE callback:

```cpp
[]() { UI::showBleActive(WifiMgr::isBleClientConnected()); }
```

---

## Summary of File Changes

| File | Change |
|------|--------|
| `include/config.h.example` | Remove `WIFI_SSID_DEFAULT` and `WIFI_PASS_DEFAULT` defines |
| `include/wifi_mgr.h` | Add `hasCredentials()`, `isBleClientConnected()` |
| `include/ui.h` | Change `showBleActive()` → `showBleActive(bool connected)`; add `showNoCredentials()` |
| `src/wifi_mgr.cpp` | `loadCredentials()` no fallback; add `hasCredentials()`; `isBleClientConnected()`; `BleTxSubscribeCb`; `s_bleSubscribed` flag; `sendBanner()`; ANSI prompts; empty-pass accept; empty-cred guard in `beginAttempt()` |
| `src/ui.cpp` | `showBleActive(bool)` with WAITING/CONNECTED; add `showNoCredentials()` |
| `src/main.cpp` | Empty-NVS check in `setup()`; update BLE callback signature |

---

## Testing

1. **Flash with no NVS credentials**: should show "No WiFi Setup / Press to continue" → encoder press → WiFi menu
2. **BLE setup happy path**: connect in Bluefruit → open UART tab → banner appears → enter SSID + password → Y → reboot → connects
3. **BLE disconnect mid-SSID**: disconnect during SSID entry → reconnect + open UART → banner fires again from top
4. **BLE empty password**: enter SSID → press Enter on blank password → confirm shows `Pass: (none)` → Y → saves → reboots → connects to open network
5. **Credential rotation**: enter new credentials over BLE → reboot → boot status uses new creds
6. **`config.h.example`**: confirm `grep WIFI_SSID_DEFAULT` returns nothing — no compiled-in credentials
